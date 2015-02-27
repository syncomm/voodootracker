/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       	 		- XM Player -				 	 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/ 

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "xm-player.h"
#include "xm.h"
#include "main.h"
#include "driver.h"
#include "audio.h"

int player_songpos, player_patpos;
static double current_time;
static int xmplayer_playmode;

enum {
    PLAYING_SONG = 1,
    PLAYING_PATTERN,
    PLAYING_NOTE,
};

#define PITCH_NOTE        (16 * 4 * 4)
#define PITCH_OCTAVE      (12 * PITCH_NOTE) 

static inline int
env_length (STEnvelope *env)
{
    return env->points[env->num_points - 1].pos;
}

static int
env_interpolate (int v1, int v2, int p1, int p, int p2)
{
    g_assert(v1 >= 0 && v1 <= 64);
    g_assert(v2 >= 0 && v2 <= 64);
    g_assert(p2 > p1);
    g_assert(p >= p1 && p < p2);

    return (p - p1) * (v2 - v1) / (p2 - p1);
}

static int
env_handle (STEnvelope *env, guint32 *p, guint8 sustain)
{
    int i, v;

    /* this happens sometimes in KB's "m6v-tlb.xm". i think it's a bug in the player somewhere. */
    if(*p > env_length(env))
	*p = env_length(env);

    for(i = env->num_points - 1; i >= 1; i--) {
	if(env->points[i].pos <= *p)
	    break;
    }

    v = env->points[i].val;
    if(*p != env->points[i].pos) {
	v += env_interpolate(env->points[i].val, env->points[i+1].val, env->points[i].pos, *p, env->points[i+1].pos);
    }

    if(*p < env_length(env) && !(sustain && (env->flags & EF_SUSTAIN) && *p == env->points[env->sustain_point].pos)) {
	*p += 1;
	if(env->flags & EF_LOOP) {
	    if(*p == env->points[env->loop_end].pos
	       /* this is weird... but it is one of KB's latest fixes which I don't understand. Let's trust him. */
	       && (sustain || !(env->flags & EF_SUSTAIN) || (env->points[env->loop_end].pos != env->points[env->sustain_point].pos))) {
		*p = env->points[env->loop_start].pos;
	    }
	}
    }

    return 4 * v;
}

static inline double
pitch_to_freq (int pitch)
{
    return 8363 * pow(2,( (float)(- pitch) / (float)PITCH_OCTAVE));
}

static inline guint32
umulshr16 (guint32 a,
	   guint32 b)
{
/* #pragma aux umulshr16 parm [eax] [edx] [ebx] value [eax] = "mul edx" "shrd eax,edx,16" */
    unsigned long long c = (unsigned long long)a * (unsigned long long)b;
    return c >> 16;
}

static inline guint32
umuldiv (guint32 a,
	 guint32 b,
	 guint32 c)
{
/* #pragma aux umuldiv parm [eax] [edx] [ebx] value [eax] = "mul edx" "div ebx" */
    unsigned long long d = (unsigned long long)a * (unsigned long long)b;
    return d / c;
}

enum
{
  xmpCmdArpeggio=0,xmpCmdPortaU=1,xmpCmdPortaD=2,xmpCmdPortaNote=3,
  xmpCmdVibrato=4,xmpCmdPortaVol=5,xmpCmdVibVol=6,xmpCmdTremolo=7,
  xmpCmdPanning=8,xmpCmdOffset=9,xmpCmdVolSlide=10,xmpCmdJump=11,
  xmpCmdVolume=12,xmpCmdBreak=13,xmpCmdSpeed=15,xmpCmdGVolume=16,
  xmpCmdGVolSlide=17,xmpCmdKeyOff=20,xmpCmdEnvPos=21,xmpCmdPanSlide=25,
  xmpCmdMRetrigger=27,xmpCmdTremor=29,xmpCmdXPorta=33,xmpCmpSFilter=36,
  xmpCmdFPortaU=37,xmpCmdFPortaD=38,xmpCmdGlissando=39,xmpCmdVibType=40,
  xmpCmdSFinetune=41,xmpCmdPatLoop=42,xmpCmdTremType=43,xmpCmdSPanning=44,
  xmpCmdRetrigger=45,xmpCmdFVolSlideU=46, xmpCmdFVolSlideD=47,
  xmpCmdNoteCut=48,xmpCmdDelayNote=49,xmpCmdPatDelay=50, xmpCmdSync1=28,
  xmpCmdSync2=32, xmpCmdSync3=51,
  xmpVCmdVol0x=1,xmpVCmdVol1x=2,xmpVCmdVol2x=3,xmpVCmdVol3x=4,xmpVCmdVol40=5,
  xmpVCmdVolSlideD=6,xmpVCmdVolSlideU=7,xmpVCmdFVolSlideD=8,
  xmpVCmdFVolSlideU=9,xmpVCmdVibRate=10,xmpVCmdVibDep=11,xmpVCmdPanning=12,
  xmpVCmdPanSlideL=13,xmpVCmdPanSlideR=14,xmpVCmdPortaNote=15,
  xmpCmdMODtTempo=128,
};

typedef struct channel
{
    int chVol;
    int chFinalVol;
    int chPan;
    int chFinalPan;
    gint32 chPitch;          /* Pitch really means 'period' in nonlinear and module frequencies */
    gint32 chFinalPitch;
    int curnote;

    guint8 chCurIns;
    guint8 chLastIns;
    int chCurNormNote;
    guint8 chSustain;
    guint16 chFadeVol;
    guint16 chAVibPos;
    guint32 chAVibSwpPos;
    guint32 chVolEnvPos;
    guint32 chPanEnvPos;

    guint8 chDefVol;
    int chDefPan;
    guint8 chCommand;
    guint8 chVCommand;
    gint32 chPortaToPitch;
    gint32 chPortaToVal;
    guint8 chVolSlideVal;
    guint8 chGVolSlideVal;
    guint8 chVVolPanSlideVal;
    guint8 chPanSlideVal;
    guint8 chFineVolSlideUVal;
    guint8 chFineVolSlideDVal;
    gint32 chPortaUVal;
    gint32 chPortaDVal;
    guint8 chFinePortaUVal;
    guint8 chFinePortaDVal;
    guint8 chXFinePortaUVal;
    guint8 chXFinePortaDVal;
    guint8 chVibRate;
    guint8 chVibPos;
    guint8 chVibType;
    guint8 chVibDep;
    guint8 chTremRate;
    guint8 chTremPos;
    guint8 chTremType;
    guint8 chTremDep;
    guint8 chPatLoopCount;
    guint8 chPatLoopStart;
    guint8 chArpPos;
    guint8 chArpOffsets[3];
    guint8 chActionTick;
    guint8 chMRetrigPos;
    guint8 chMRetrigLen;
    guint8 chMRetrigAct;
    guint8 chDelayNote;
    guint8 chDelayIns;
    guint8 chDelayVol;
    guint8 chOffset;
    guint8 chGlissando;
    guint8 chTremorPos;
    guint8 chTremorLen;
    guint8 chTremorOff;

    int nextstop;
    STSample *nextsamp;
    int nextsampid;
    int nextpos;
    STSample *cursamp;
    int cursampid;
    STInstrument *curins;
} channel;

static channel channels[32];

static guint8 globalvol;

static guint8 curtick;
static guint8 curtempo;
static guint8 tick0;

static int currow;
static XMPattern *curpattern;
static int patlen;
static int curord;

static int nord;
static int ninst;
static int nsamp;
static int linearfreq;
static int nchan;
static int loopord;
static int ismod;

static int jumptoord;
static int jumptorow;
static int patdelay;

static guint8 procnot;
static guint8 procins;
static guint8 procvol;
static guint8 proccmd;
static guint8 procdat;
static int curbpm;

static int realgvol;

static gint16 sintab[256]=
{
      0,    50,   100,   151,   201,   251,   301,   350,
    400,   449,   498,   546,   595,   642,   690,   737,
    784,   830,   876,   921,   965,  1009,  1053,  1096,
   1138,  1179,  1220,  1260,  1299,  1338,  1375,  1412,
   1448,  1483,  1517,  1551,  1583,  1615,  1645,  1674,
   1703,  1730,  1757,  1782,  1806,  1829,  1851,  1872,
   1892,  1911,  1928,  1945,  1960,  1974,  1987,  1998,
   2009,  2018,  2026,  2033,  2038,  2042,  2046,  2047,
   2048,  2047,  2046,  2042,  2038,  2033,  2026,  2018,
   2009,  1998,  1987,  1974,  1960,  1945,  1928,  1911,
   1892,  1872,  1851,  1829,  1806,  1782,  1757,  1730,
   1703,  1674,  1645,  1615,  1583,  1551,  1517,  1483,
   1448,  1412,  1375,  1338,  1299,  1260,  1220,  1179,
   1138,  1096,  1053,  1009,   965,   921,   876,   830,
    784,   737,   690,   642,   595,   546,   498,   449,
    400,   350,   301,   251,   201,   151,   100,    50,
      0,   -50,  -100,  -151,  -201,  -251,  -301,  -350,
   -400,  -449,  -498,  -546,  -595,  -642,  -690,  -737,
   -784,  -830,  -876,  -921,  -965, -1009, -1053, -1096,
  -1138, -1179, -1220, -1260, -1299, -1338, -1375, -1412,
  -1448, -1483, -1517, -1551, -1583, -1615, -1645, -1674,
  -1703, -1730, -1757, -1782, -1806, -1829, -1851, -1872,
  -1892, -1911, -1928, -1945, -1960, -1974, -1987, -1998,
  -2009, -2018, -2026, -2033, -2038, -2042, -2046, -2047,
  -2048, -2047, -2046, -2042, -2038, -2033, -2026, -2018,
  -2009, -1998, -1987, -1974, -1960, -1945, -1928, -1911,
  -1892, -1872, -1851, -1829, -1806, -1782, -1757, -1730,
  -1703, -1674, -1645, -1615, -1583, -1551, -1517, -1483,
  -1448, -1412, -1375, -1338, -1299, -1260, -1220, -1179,
  -1138, -1096, -1053, -1009,  -965,  -921,  -876,  -830,
   -784,  -737,  -690,  -642,  -595,  -546,  -498,  -449,
   -400,  -350,  -301,  -251,  -201,  -151,  -100,   -50
};

static guint32 hnotetab6848[16]={11131415,4417505,1753088,695713,276094,109568,43482,17256,6848,2718,1078,428,170,67,27,11};
static guint32 hnotetab8363[16]={13594045,5394801,2140928,849628,337175,133808,53102,21073,8363,3319,1317,523,207,82,33,13};
static guint16 notetab[16]={32768,30929,29193,27554,26008,24548,23170,21870,20643,19484,18390,17358,16384,15464,14596,13777};
static guint16 finetab[16]={32768,32650,32532,32415,32298,32182,32066,31950,31835,31720,31606,31492,31379,31266,31153,31041};
static guint16 xfinetab[16]={32768,32761,32753,32746,32738,32731,32724,32716,32709,32702,32694,32687,32679,32672,32665,32657};

/* This table ripped from pt-play.s (the original PT replayer) by Crayon/Noxious */
static guint16 protracker_periods[16][36] = {
    { // Tuning -8
	907,856,808,762,720,678,640,604,570,538,508,480,
	453,428,404,381,360,339,320,302,285,269,254,240,
	226,214,202,190,180,170,160,151,143,135,127,120,
    },
    { // Tuning -7
	900,850,802,757,715,675,636,601,567,535,505,477,
	450,425,401,379,357,337,318,300,284,268,253,238,
	225,212,200,189,179,169,159,150,142,134,126,119,
    },
    { // Tuning -6
	894,844,796,752,709,670,632,597,563,532,502,474,
	447,422,398,376,355,335,316,298,282,266,251,237,
	223,211,199,188,177,167,158,149,141,133,125,118,
    },
    { // Tuning -5
	887,838,791,746,704,665,628,592,559,528,498,470,
	444,419,395,373,352,332,314,296,280,264,249,235,
	222,209,198,187,176,166,157,148,140,132,125,118,
    },
    { // Tuning -4
	881,832,785,741,699,660,623,588,555,524,494,467,
	441,416,392,370,350,330,312,294,278,262,247,233,
	220,208,196,185,175,165,156,147,139,131,123,117,
    },
    { // Tuning -3
	875,826,779,736,694,655,619,584,551,520,491,463,
	437,413,390,368,347,328,309,292,276,260,245,232,
	219,206,195,184,174,164,155,146,138,130,123,116,
    },
    { // Tuning -2
	868,820,774,730,689,651,614,580,547,516,487,460,
	434,410,387,365,345,325,307,290,274,258,244,230,
	217,205,193,183,172,163,154,145,137,129,122,115,
    },
    { // Tuning -1
	862,814,768,725,684,646,610,575,543,513,484,457,
	431,407,384,363,342,323,305,288,272,256,242,228,
	216,203,192,181,171,161,152,144,136,128,121,114,
    },
    { // Tuning 0, Normal
	856,808,762,720,678,640,604,570,538,508,480,453,
	428,404,381,360,339,320,302,285,269,254,240,226,
	214,202,190,180,170,160,151,143,135,127,120,113,
    },
    { // Tuning 1
	850,802,757,715,674,637,601,567,535,505,477,450,
	425,401,379,357,337,318,300,284,268,253,239,225,
	213,201,189,179,169,159,150,142,134,126,119,113,
    },
    { // Tuning 2
	844,796,752,709,670,632,597,563,532,502,474,447,
	422,398,376,355,335,316,298,282,266,251,237,224,
	211,199,188,177,167,158,149,141,133,125,118,112,
    },
    { // Tuning 3
	838,791,746,704,665,628,592,559,528,498,470,444,
	419,395,373,352,332,314,296,280,264,249,235,222,
	209,198,187,176,166,157,148,140,132,125,118,111,
    },
    { // Tuning 4
	832,785,741,699,660,623,588,555,524,495,467,441,
	416,392,370,350,330,312,294,278,262,247,233,220,
	208,196,185,175,165,156,147,139,131,124,117,110,
    },
    { // Tuning 5
	826,779,736,694,655,619,584,551,520,491,463,437,
	413,390,368,347,328,309,292,276,260,245,232,219,
	206,195,184,174,164,155,146,138,130,123,116,109,
    },
    { // Tuning 6
	820,774,730,689,651,614,580,547,516,487,460,434,
	410,387,365,345,325,307,290,274,258,244,230,217,
	205,193,183,172,163,154,145,137,129,122,115,109,
    },
    { // Tuning 7
	814,768,725,684,646,610,575,543,513,484,457,431,
	407,384,363,342,323,305,288,272,256,242,228,216,
	204,192,181,171,161,152,144,136,128,121,114,108,
    }
};

static int mcpGetFreq6848(int note)
{
    note=-note;
    return umulshr16(umulshr16(umulshr16(hnotetab6848[((note+0x8000)>>12)&0xF],notetab[(note>>8)&0xF]*2),finetab[(note>>4)&0xF]*2),xfinetab[note&0xF]*2);
}

static int mcpGetNote6848(int frq)
{
    gint16 x;
    int i;
    for (i=0; i<15; i++)
	if (hnotetab6848[i+1]<frq)
	    break;
    x=(i-8)*16*256;
    frq=umuldiv(frq, 32768, hnotetab6848[i]);
    for (i=0; i<15; i++)
	if (notetab[i+1]<frq)
	    break;
    x+=i*256;
    frq=umuldiv(frq, 32768, notetab[i]);
    for (i=0; i<15; i++)
	if (finetab[i+1]<frq)
	    break;
    x+=i*16;
    frq=umuldiv(frq, 32768, finetab[i]);
    for (i=0; i<15; i++)
	if (xfinetab[i+1]<frq)
	    break;
    return -x-i;
}

int mcpGetNote8363(int frq)
{
    gint16 x;
    int i;
    for (i=0; i<15; i++)
	if (hnotetab8363[i+1]<frq)
	    break;
    x=(i-8)*16*256;
    frq=umuldiv(frq, 32768, hnotetab8363[i]);
    for (i=0; i<15; i++)
	if (notetab[i+1]<frq)
	    break;
    x+=i*256;
    frq=umuldiv(frq, 32768, notetab[i]);
    for (i=0; i<15; i++)
	if (finetab[i+1]<frq)
	    break;
    x+=i*16;
    frq=umuldiv(frq, 32768, finetab[i]);
    for (i=0; i<15; i++)
	if (xfinetab[i+1]<frq)
	    break;
    return -x-i;
}

static int freqrange(int x)
{
    if(ismod) {
	/* Values from ProTracker 2.2a documentation */
	return CLAMP(x, 113 << 4, 856 << 4);
    } else {
	if (linearfreq)
	    return (x<-72*256)?-72*256:(x>96*256)?96*256:x;
	else
	    return (x<107)?107:(x>438272)?438272:x;
    }
}

static int volrange(int x)
{
    return (x<0)?0:(x>0x40)?0x40:x;
}

static int panrange(int x)
{
    return (x<0)?0:(x>0xFF)?0xFF:x;
}

static int
xm_player_start_note (channel *ch,
		      int note)
{
    STInstrument *ins = &xm->instruments[ch->chCurIns-1];
    note--;
    if(ins->samplemap[note] > nsamp)
	return 0;
    ch->curins = ins;
    ch->cursamp = &ins->samples[ins->samplemap[note]];
    ch->cursampid = (ch->chCurIns - 1) * 16 + ins->samplemap[note];
    ch->chDefVol = ch->cursamp->volume;
    ch->chDefPan = ch->cursamp->panning;
    return 1;
}

static gint32
xm_player_get_note_frq (channel *ch)
{
    if(!ismod) {
	gint32 frq = 48*256 - (((procnot - 1) << 8) - ch->chCurNormNote);
	
	if(linearfreq)
	    return frq;
	else
	    return mcpGetFreq6848(frq);
    } else {
	int note = procnot - 1 - 36;

	if(note < 0)
	    note = 0;
	else if(note >= 36)
	    note = 35;
	
	return protracker_periods[(ch->cursamp->finetune >> 4) + 8][note] << 4;
    }
}

static void
xm_player_playnote_protracker (channel *ch)
{
    int portatmp=0;
    int delaytmp;

    if (proccmd==xmpCmdPortaNote)
	portatmp=1;
    if (proccmd==xmpCmdPortaVol)
	portatmp=1;

    delaytmp=(proccmd==xmpCmdDelayNote)&&procdat;

    if (!ch->chCurIns)
	return;

/* This needs to be fixed! */
    if (!procnot && procins && ch->chCurIns!=ch->chLastIns)
	procnot=ch->curnote;

    if (procnot && !delaytmp)
	ch->curnote=procnot;

    if (procins) {
	gint32 checknote = ch->curnote;
	if(!checknote)
	    checknote = 49;
	if(!xm_player_start_note(ch, checknote))
	    return;
    }
    
    if (procnot && !delaytmp) {
	if (!portatmp) {
	    gint32 nn;
	    ch->nextstop=1;

	    ch->nextsamp = ch->cursamp;
	    ch->nextsampid = ch->cursampid;

	    /* CurNormNote is only relevant in FastTracker mode */
	    nn = -ch->cursamp->relnote*256 - ch->cursamp->finetune*2;
	    if(proccmd == xmpCmdSFinetune)
		nn = -ch->cursamp->relnote*256 - (gint16)(procdat << 4) + 0x80;
	    ch->chCurNormNote = nn;
		
	    ch->chPitch = ch->chFinalPitch = ch->chPortaToPitch = xm_player_get_note_frq(ch);

	    ch->nextpos=0;

	    if (proccmd==xmpCmdOffset) {
		if (procdat!=0)
		    ch->chOffset=procdat;
		ch->nextpos=ch->chOffset<<8;
		if (1 && ch->nextpos > ch->nextsamp->length)
		    ch->nextpos = ch->nextsamp->length - 16;
	    }

	    ch->chVibPos=0;
	    ch->chTremPos=0;
	    ch->chArpPos=0;
	    ch->chMRetrigPos=0;
	    ch->chTremorPos=0;
	} else {
	    ch->chPortaToPitch = xm_player_get_note_frq(ch);
	}
    }

    if (procins) {
	ch->chVol=ch->chDefVol;
	ch->chFinalVol=ch->chDefVol;
	if (ch->chDefPan!=-1)
	    {
		ch->chPan=ch->chDefPan;
		ch->chFinalPan=ch->chDefPan;
	    }
	ch->chFadeVol=0x8000;
	ch->chAVibPos=0;
	ch->chAVibSwpPos=0;
	ch->chVolEnvPos=0;
	ch->chPanEnvPos=0;
    }
}

static void
xm_player_playnote_fasttracker (channel *ch)
{
    int portatmp=0;
    int delaytmp;
    int keyoff=0;

    if (proccmd==xmpCmdPortaNote)
	portatmp=1;
    if (proccmd==xmpCmdPortaVol)
	portatmp=1;
    if ((procvol>>4)==xmpVCmdPortaNote)
	portatmp=1;

    delaytmp=(proccmd==xmpCmdDelayNote)&&procdat;

    if (procnot==97) {
	procnot=0;
	keyoff=1;
    }

    if ((proccmd==xmpCmdKeyOff)&&!procdat)
	keyoff=1;

    if (!ch->chCurIns)
	return;

    if (procins && !keyoff && !delaytmp)
	ch->chSustain=1;

    if (procnot && !delaytmp)
	ch->curnote=procnot;

    if (procins && !delaytmp) {
	gint32 checknote = ch->curnote;
	if(!checknote)
	    checknote = 49;
	if(!xm_player_start_note(ch, checknote))
	    return;
    }
    
    if (procnot && !delaytmp) {
	if (!portatmp) {
	    gint32 nn;
	    ch->nextstop=1;

	    if(procins) {
		if(!xm_player_start_note(ch, ch->curnote))
		    return;
	    }

	    ch->nextsamp = ch->cursamp;
	    ch->nextsampid = ch->cursampid;

	    /* CurNormNote is only relevant in FastTracker mode */
	    nn = -ch->cursamp->relnote*256 - ch->cursamp->finetune*2;
	    if(proccmd == xmpCmdSFinetune)
		nn = -ch->cursamp->relnote*256 - (gint16)(procdat << 4) + 0x80;
	    ch->chCurNormNote = nn;
		
	    ch->chPitch = ch->chFinalPitch = ch->chPortaToPitch = xm_player_get_note_frq(ch);

	    ch->nextpos=0;

	    if (proccmd==xmpCmdOffset) {
		if (procdat!=0)
		    ch->chOffset=procdat;
		ch->nextpos=ch->chOffset<<8;
	    }

	    ch->chVibPos=0;
	    ch->chTremPos=0;
	    ch->chArpPos=0;
	    ch->chMRetrigPos=0;
	    ch->chTremorPos=0;
	} else {
	    ch->chPortaToPitch = xm_player_get_note_frq(ch);
	}
    }

    if (procnot && delaytmp)
	return;

    if (keyoff&&ch->cursamp) {
	ch->chSustain=0;
	if (!(ch->curins->vol_env.flags & EF_ON) && !procins)
	    ch->chFadeVol=0;
    }

    if (procins && ch->chSustain) {
	ch->chVol=ch->chDefVol;
	ch->chFinalVol=ch->chDefVol;
	if (ch->chDefPan!=-1)
	    {
		ch->chPan=ch->chDefPan;
		ch->chFinalPan=ch->chDefPan;
	    }
	ch->chFadeVol=0x8000;
	ch->chAVibPos=0;
	ch->chAVibSwpPos=0;
	ch->chVolEnvPos=0;
	ch->chPanEnvPos=0;
    }
}

static void
PlayNote (int chnr)
{
    channel *ch = &channels[chnr];

    if(ismod)
	xm_player_playnote_protracker(ch);
    else
	xm_player_playnote_fasttracker(ch);
}

static void
xmplayer_final_channel_ops (int chnr)
{
    int vol, pan;
    channel *ch = &channels[chnr];

    if(player_mute_channels[chnr]) {
	driver_setvolume(chnr, 0);
	return;
    }

    vol=(ch->chFinalVol*globalvol)>>4;
    pan=ch->chFinalPan-128;

    if(!ismod) {
	if (!ch->chSustain) {
	    vol=(vol*ch->chFadeVol)>>15;
	    if (ch->chFadeVol >= ch->curins->volfade)
		ch->chFadeVol -= ch->curins->volfade;
	    else
		ch->chFadeVol=0;
	}

	if(ch->curins->vol_env.flags & EF_ON) {
	    vol = (vol * env_handle(&ch->curins->vol_env, &ch->chVolEnvPos, ch->chSustain)) >> 8;
	}
	
	if(ch->curins->pan_env.flags & EF_ON) {
	    pan += ((env_handle(&ch->curins->pan_env, &ch->chPanEnvPos, ch->chSustain)-128)*(128-((pan<0)?-pan:pan)))>>7;
	}

	if(ch->curins->vibrate && ch->curins->vibdepth) {
	    int dep=0;
	    switch (ch->curins->vibtype) {
	    case 0:
		dep=(sintab[ch->chAVibPos>>8] * (ch->curins->vibdepth << 2))>>11;
		break;
	    case 1:
		dep=(ch->chAVibPos&0x8000)? -(ch->curins->vibdepth << 2) : (ch->curins->vibdepth << 2);
		break;
	    case 2:
		dep=((ch->curins->vibdepth << 2)*(32768-ch->chAVibPos))>>14;
		break;
	    case 3:
		dep=((ch->curins->vibdepth << 2)*(ch->chAVibPos-32768))>>14;
		break;
	    }
	    
	    ch->chAVibSwpPos += 0xFFFF / (ch->curins->vibsweep + 1);
	    if (ch->chAVibSwpPos>0x10000)
		ch->chAVibSwpPos=0x10000;
	    dep=(dep*(int)ch->chAVibSwpPos)>>17;
	    
	    ch->chFinalPitch-=dep;
	    
	    ch->chAVibPos += ch->curins->vibrate << 8;
	}
    }

    if(ch->nextstop) {
	driver_stopnote(chnr);
    }
    if(ch->nextsamp != NULL) {
	driver_startnote(chnr, ch->nextsampid);
    }
    if(ch->nextpos != -1) {
	driver_setsmplpos(chnr, ch->nextpos);
    }
    if(ismod) {
	if(ch->chFinalPitch != 0) { /* == 0 happens on tru_funk.mod */
	    /* PAL clock constant is 3546895, NTSC clock constant is 3579545 */
	    /* Taken from "Amiga Hardware Reference Manual, revised & updated", September 1989 printing */
	    driver_setfreq(chnr, (double)(3546895 * 16) / ch->chFinalPitch);
	}
    } else {
	if(linearfreq) {
	    driver_setfreq(chnr, pitch_to_freq(ch->chFinalPitch));
	} else {
	    if(ch->chFinalPitch != 0) { /* == 0 happens on tru_funk.mod */
		driver_setfreq(chnr, pitch_to_freq(-mcpGetNote8363(8363*6848/ch->chFinalPitch)));
	    }
	}
    }

    driver_setvolume(chnr, (double)vol / 4 / 64);

    if(ismod) {
	driver_setpanning(chnr, chnr == 0 || chnr == 3 ? -1.0 : +1.0);
    } else {
	driver_setpanning(chnr, (double)pan / 128);
    }
}

static gint32
xm_player_handle_glissando (channel *ch)
{
    if(ch->chGlissando) {
	if(ismod) {
	    fprintf(stderr, "Glissando (E31) for ProTracker modules not supported yet (in module '%s').\n", xm->name);
	    return ch->chPitch;
	} else {
	    if(linearfreq)
		return ((ch->chPitch+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote;
	    else
		return mcpGetFreq6848(((mcpGetNote6848(ch->chPitch)+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote);
	}
    } else
	return ch->chPitch;
}

static void xmpPlayTick()
{
    int i;

    tick0=0;

    for (i=0; i<nchan; i++) {
	channel *ch=&channels[i];
	ch->chFinalVol=ch->chVol;
	ch->chFinalPan=ch->chPan;
	ch->chFinalPitch=ch->chPitch;
	ch->nextstop=0;
	ch->nextsamp=NULL;
	ch->nextpos=-1;
    }

    if(xmplayer_playmode == PLAYING_NOTE) {
	for(i = 0; i < nchan; i++) {
	    channel *ch = &channels[i];
	    if (!ch->cursamp) {
		driver_stopnote(i);
	    } else {
		xmplayer_final_channel_ops(i);
	    }
	}
	return;
    }

    curtick++;
    if (curtick>=curtempo)
	curtick=0;

    if (!curtick&&patdelay) {
	if (jumptoord!=-1) {
	    if (jumptoord!=curord)
		for (i=0; i<nchan; i++) {
		    channel *ch=&channels[i];
		    ch->chPatLoopCount=0;
		    ch->chPatLoopStart=0;
		}

	    if (jumptoord>=nord) {
		jumptoord=loopord;
	    }

	    if(xmplayer_playmode == PLAYING_SONG) {
		curord=jumptoord;
		patlen = xm->patterns[xm->pattern_order_table[curord]].length;
		curpattern = &xm->patterns[xm->pattern_order_table[curord]];
	    }
	    currow=jumptorow;
	    jumptoord=-1;
	}
    }

    if (!curtick && (!patdelay || ismod)) {
	tick0=1;

	if (!patdelay) {
	    currow++;
	    if ((jumptoord==-1)&&(currow>=patlen)) {
		jumptoord=curord+1;
		jumptorow=0;
	    }
	    if (jumptoord!=-1) {
		if (jumptoord!=curord)
		    for (i=0; i<nchan; i++) {
			channel *ch=&channels[i];
			ch->chPatLoopCount=0;
			ch->chPatLoopStart=0;
		    }

		if (jumptoord>=nord)
		    jumptoord=loopord;

		if(xmplayer_playmode == PLAYING_SONG) {
		    curord=jumptoord;
		    patlen = xm->patterns[xm->pattern_order_table[curord]].length;
		    curpattern = &xm->patterns[xm->pattern_order_table[curord]];
		}
		currow=jumptorow;
		jumptoord=-1;
	    }
	}

	for (i=0; i<nchan; i++) {
	    channel *ch=&channels[i];

	    procnot = curpattern->channels[i][currow].note;
	    procins = curpattern->channels[i][currow].instrument;
	    procvol = curpattern->channels[i][currow].volume;
	    proccmd = curpattern->channels[i][currow].fxtype;
	    procdat = curpattern->channels[i][currow].fxparam;

	    if(proccmd == 0xE) {
		proccmd = 36 + (procdat >> 4);
		procdat &= 0xF;
	    }

	    if (!patdelay) {
		if (procins && procins<=ninst) {
		    ch->chLastIns=ch->chCurIns;
		    ch->chCurIns=procins;
		}
		if (procins<=ninst)
		    PlayNote(i);
	    }

	    ch->chVCommand=procvol>>4;

	    switch (ch->chVCommand) {
	    case xmpVCmdVol0x: case xmpVCmdVol1x: case xmpVCmdVol2x: case xmpVCmdVol3x:
		if ((proccmd!=xmpCmdDelayNote)||!procdat)
		    ch->chFinalVol=ch->chVol=procvol-0x10;
		break;
	    case xmpVCmdVol40:
		if ((proccmd!=xmpCmdDelayNote)||!procdat)
		    ch->chFinalVol=ch->chVol=0x40;
		break;
	    case xmpVCmdVolSlideD: case xmpVCmdVolSlideU: case xmpVCmdPanSlideL: case xmpVCmdPanSlideR:
		ch->chVVolPanSlideVal=procvol&0xF;
		break;
	    case xmpVCmdFVolSlideD:
		if ((proccmd!=xmpCmdDelayNote)||!procdat)
		    ch->chFinalVol=ch->chVol=volrange(ch->chVol-(procvol&0xF));
		break;
	    case xmpVCmdFVolSlideU:
		if ((proccmd!=xmpCmdDelayNote)||!procdat)
		    ch->chFinalVol=ch->chVol=volrange(ch->chVol+(procvol&0xF));
		break;
	    case xmpVCmdVibRate:
		if (procvol&0xF)
		    ch->chVibRate=((procvol&0xF)<<2);
		break;
	    case xmpVCmdVibDep:
		if (procvol&0xF)
		    ch->chVibDep=((procvol&0xF)<<1);
		break;
	    case xmpVCmdPanning:
		if ((proccmd!=xmpCmdDelayNote)||!procdat)
		    ch->chFinalPan=ch->chPan=(procvol&0xF)*0x11;
		break;
	    case xmpVCmdPortaNote:
		if (procvol&0xF)
		    ch->chPortaToVal=(procvol&0xF)<<8;
		break;
	    }

	    ch->chCommand=proccmd;

	    switch (ch->chCommand) {
	    case xmpCmdArpeggio:
		if (!procdat)
		    ch->chCommand=0xFF;
		ch->chArpOffsets[0]=0;
		ch->chArpOffsets[1]=procdat>>4;
		ch->chArpOffsets[2]=procdat&0xF;
		break;
	    case xmpCmdPortaU:
		if (procdat)
		    ch->chPortaUVal=procdat<<4;
		break;
	    case xmpCmdPortaD:
		if (procdat)
		    ch->chPortaDVal=procdat<<4;
		break;
	    case xmpCmdPortaNote:
		if (procdat)
		    ch->chPortaToVal=procdat<<4;
		break;
	    case xmpCmdVibrato:
		if (procdat&0xF)
		    ch->chVibDep=(procdat&0xF)<<1;
		if (procdat&0xF0)
		    ch->chVibRate=(procdat>>4)<<2;
		break;
	    case xmpCmdPortaVol: case xmpCmdVibVol: case xmpCmdVolSlide:
		if (procdat || ismod)
		    ch->chVolSlideVal=procdat;
		break;
	    case xmpCmdTremolo:
		if (procdat&0xF)
		    ch->chTremDep=(procdat&0xF)<<2;
		if (procdat&0xF0)
		    ch->chTremRate=(procdat>>4)<<2;
		break;
	    case xmpCmdPanning:
		ch->chFinalPan=ch->chPan=procdat;
		break;
	    case xmpCmdJump:
		if (!patdelay) {
		    jumptoord=procdat;
		    jumptorow=0;
		}
		break;
	    case xmpCmdVolume:
		ch->chFinalVol=ch->chVol=volrange(procdat);
		break;
	    case xmpCmdBreak:
		if (!patdelay) {
		    if (jumptoord==-1)
			jumptoord=curord+1;
		    jumptorow=(procdat&0xF)+(procdat>>4)*10;
		}
		break;
	    case xmpCmdSpeed:
		if (!procdat) {
		    jumptoord=procdat;
		    jumptorow=0;
		    break;
		}
		if (procdat>=0x20) {
		    curbpm=procdat;
		} else {
		    curtempo=procdat;
		}
		break;
	    case xmpCmdMODtTempo:
		if (!procdat) {
		    jumptoord=procdat;
		    jumptorow=0;
		} else {
		    curtempo=procdat;
		}
		break;
	    case xmpCmdGVolume:
		globalvol=volrange(procdat);
		break;
	    case xmpCmdGVolSlide:
		if (procdat)
		    ch->chGVolSlideVal=procdat;
		break;
	    case xmpCmdKeyOff:
		ch->chActionTick=procdat;
		break;
	    case xmpCmdRetrigger:
		ch->chActionTick=procdat;
		break;
	    case xmpCmdNoteCut:
		ch->chActionTick=procdat;
		break;
	    case xmpCmdEnvPos:
		ch->chVolEnvPos=ch->chPanEnvPos=procdat;
		if (ch->curins->vol_env.flags & EF_ON)
		    if (ch->chVolEnvPos > env_length(&ch->curins->vol_env))
			ch->chVolEnvPos = env_length(&ch->curins->vol_env);
		if (ch->curins->pan_env.flags & EF_ON)
		    if (ch->chPanEnvPos > env_length(&ch->curins->pan_env))
			ch->chPanEnvPos = env_length(&ch->curins->pan_env);
		break;
	    case xmpCmdPanSlide:
		if (procdat)
		    ch->chPanSlideVal=procdat;
		break;
	    case xmpCmdMRetrigger:
		if (procdat) {
		    ch->chMRetrigLen=procdat&0xF;
		    ch->chMRetrigAct=procdat>>4;
		}
		break;
	    case xmpCmdSync1: case xmpCmdSync2: case xmpCmdSync3:
		break;
	    case xmpCmdTremor:
		if (procdat) {
		    ch->chTremorLen=(procdat&0xF)+(procdat>>4)+2;
		    ch->chTremorOff=(procdat>>4)+1;
		    ch->chTremorPos=0;
		}
		break;
	    case xmpCmdXPorta:
		if ((procdat>>4)==1) {
		    if (procdat&0xF)
			ch->chXFinePortaUVal=procdat&0xF;
		    ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-(ch->chXFinePortaUVal<<2));
		} else if ((procdat>>4)==2) {
		    if (procdat&0xF)
			ch->chXFinePortaDVal=procdat&0xF;
		    ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+(ch->chXFinePortaDVal<<2));
		}
		break;
	    case xmpCmdFPortaU:
		if (procdat || ismod)
		    ch->chFinePortaUVal=procdat;
		ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-(ch->chFinePortaUVal<<4));
		break;
	    case xmpCmdFPortaD:
		if (procdat || ismod)
		    ch->chFinePortaDVal=procdat;
		ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+(ch->chFinePortaDVal<<4));
		break;
	    case xmpCmdGlissando:
		ch->chGlissando=procdat;
		break;
	    case xmpCmdVibType:
		ch->chVibType=procdat&3;
		break;
	    case xmpCmdPatLoop:
		if (!procdat)
		    ch->chPatLoopStart=currow;
		else {
		    ch->chPatLoopCount++;
		    if (ch->chPatLoopCount<=procdat) {
			jumptorow=ch->chPatLoopStart;
			jumptoord=curord;
		    } else {
			ch->chPatLoopCount=0;
			ch->chPatLoopStart=currow+1;
		    }
		}
		break;
	    case xmpCmdTremType:
		ch->chTremType=procdat&3;
		break;
	    case xmpCmdSPanning:
		ch->chFinalPan=ch->chPan=procdat*0x11;
		break;
	    case xmpCmdFVolSlideU:
		if (procdat || ismod)
		    ch->chFineVolSlideUVal=procdat;
		ch->chFinalVol=ch->chVol=volrange(ch->chVol+ch->chFineVolSlideUVal);
		break;
	    case xmpCmdFVolSlideD:
		if (procdat || ismod)
		    ch->chFineVolSlideDVal=procdat;
		ch->chFinalVol=ch->chVol=volrange(ch->chVol-ch->chFineVolSlideDVal);
		break;
	    case xmpCmdPatDelay:
		if (!patdelay)
		    patdelay=procdat+1;
		break;
	    case xmpCmdDelayNote:
		if (procnot)
		    ch->chDelayNote=procnot;
		ch->chDelayIns=procins;
		ch->chDelayVol=procvol;
		ch->chActionTick=procdat;
		break;
	    }
	}
    }
    if (!curtick&&patdelay) {
	patdelay--;
    }

    for (i=0; i<nchan; i++) {
	channel *ch=&channels[i];

	switch (ch->chVCommand) {
	case xmpVCmdVolSlideD:
	    if (tick0)
		break;
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol-ch->chVVolPanSlideVal);
	    break;
	case xmpVCmdVolSlideU:
	    if (tick0)
		break;
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol+ch->chVVolPanSlideVal);
	    break;
	case xmpVCmdVibDep: // KB says "FICKEN" :)
	    switch (ch->chVibType) {
	    case 0:
		ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>7)+ch->chPitch);
		break;
	    case 1:
		ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>3)+ch->chPitch);
		break;
	    case 2:
		ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>2)+ch->chPitch);
		break;
	    }
	    if (!tick0)
		ch->chVibPos+=ch->chVibRate;
	    break;
	case xmpVCmdPanSlideL:
	    if (tick0)
		break;
	    ch->chFinalPan=ch->chPan=panrange(ch->chPan-ch->chVVolPanSlideVal);
	    break;
	case xmpVCmdPanSlideR:
	    if (tick0)
		break;
	    ch->chFinalPan=ch->chPan=panrange(ch->chPan+ch->chVVolPanSlideVal);
	    break;
	case xmpVCmdPortaNote:
	    if (!tick0) {
		if (ch->chPitch<ch->chPortaToPitch) {
		    ch->chPitch+=ch->chPortaToVal;
		    if (ch->chPitch>ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		} else {
		    ch->chPitch-=ch->chPortaToVal;
		    if (ch->chPitch<ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		}
	    }
	    ch->chFinalPitch = xm_player_handle_glissando(ch);
	    break;
	}

	switch (ch->chCommand) {
	case xmpCmdArpeggio:
	    if(ismod) {
		ch->chFinalPitch = freqrange((ch->chPitch * notetab[ch->chArpOffsets[curtick % 3]]) >> 15);
	    } else {
		if (linearfreq)
		    ch->chFinalPitch=freqrange(ch->chPitch-(ch->chArpOffsets[ch->chArpPos]<<8));
		else
		    ch->chFinalPitch=freqrange((ch->chPitch*notetab[ch->chArpOffsets[ch->chArpPos]])>>15);
		/* not sure if this is correct, even for XM's. One should think
		   ArpPos is equivalent to the tick counter (-mkrause). */
		ch->chArpPos++;
		if (ch->chArpPos==3)
		    ch->chArpPos=0;
	    }
	    break;
	case xmpCmdPortaU:
	    if (tick0)
		break;
	    ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-ch->chPortaUVal);
	    break;
	case xmpCmdPortaD:
	    if (tick0)
		break;
	    ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+ch->chPortaDVal);
	    break;
	case xmpCmdPortaNote:
	    if (!tick0) {
		if (ch->chPitch<ch->chPortaToPitch) {
		    ch->chPitch+=ch->chPortaToVal;
		    if (ch->chPitch>ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		} else {
		    ch->chPitch-=ch->chPortaToVal;
		    if (ch->chPitch<ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		}
	    }
	    ch->chFinalPitch = xm_player_handle_glissando(ch);
	    break;
	case xmpCmdVibrato:
	    switch (ch->chVibType) {
	    case 0:
		ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>8)+ch->chPitch);
		break;
	    case 1:
		ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>4)+ch->chPitch);
		break;
	    case 2:
		ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>3)+ch->chPitch);
		break;
	    }
	    if (!tick0)
		ch->chVibPos+=ch->chVibRate;
	    break;
	case xmpCmdPortaVol:
	    if (!tick0) {
		if (ch->chPitch<ch->chPortaToPitch) {
		    ch->chPitch+=ch->chPortaToVal;
		    if (ch->chPitch>ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		} else {
		    ch->chPitch-=ch->chPortaToVal;
		    if (ch->chPitch<ch->chPortaToPitch)
			ch->chPitch=ch->chPortaToPitch;
		}
	    }
	    ch->chFinalPitch = xm_player_handle_glissando(ch);
	    if (tick0)
		break;
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
	    break;
	case xmpCmdVibVol:
	    switch (ch->chVibType) {
	    case 0:
		ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>8)+ch->chPitch);
		break;
	    case 1:
		ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>4)+ch->chPitch);
		break;
	    case 2:
		ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>3)+ch->chPitch);
		break;
	    }
	    if (!tick0)
		ch->chVibPos+=ch->chVibRate;

	    if (tick0)
		break;
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
	    break;
	case xmpCmdTremolo:
	    switch (ch->chTremType) {
	    case 0:
		ch->chFinalVol+=(( sintab[ch->chTremPos] *ch->chTremDep)>>11);
		break;
	    case 1:
		ch->chFinalVol+=(( (ch->chTremPos-0x80)   *ch->chTremDep)>>7);
		break;
	    case 2:
		ch->chFinalVol+=(( ((ch->chTremPos&0x80)-0x40) *ch->chTremDep)>>6);
		break;
	    }
	    ch->chFinalVol=volrange(ch->chFinalVol);
	    if (!tick0)
		ch->chTremPos+=ch->chTremRate;
	    break;
	case xmpCmdVolSlide:
	    if (tick0)
		break;
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
	    break;
	case xmpCmdGVolSlide:
	    if (tick0)
		break;
	    if (ch->chGVolSlideVal&0xF0)
		globalvol=volrange(globalvol+(ch->chGVolSlideVal>>4));
	    else
		globalvol=volrange(globalvol-(ch->chGVolSlideVal&0xF));
	    break;
	case xmpCmdKeyOff:
	    if (tick0)
		break;
	    if (curtick==ch->chActionTick) {
		ch->chSustain=0;
		if (ch->cursamp && !(ch->curins->vol_env.flags & EF_ON))
		    ch->chFadeVol=0;
	    }
	    break;
	case xmpCmdPanSlide:
	    if (tick0)
		break;
	    ch->chFinalPan=ch->chPan=panrange(ch->chPan+((ch->chPanSlideVal&0xF0)?(ch->chPanSlideVal>>4):-(ch->chPanSlideVal&0xF)));
	    break;
	case xmpCmdMRetrigger:
	    if (ch->chMRetrigPos++!=ch->chMRetrigLen)
		break;
	    ch->chMRetrigPos=0;
	    ch->nextpos=0;

	    switch (ch->chMRetrigAct) {
	    case 0: case 8: break;
	    case 1: case 2: case 3: case 4: case 5:
		ch->chVol=ch->chVol-(1<<(ch->chMRetrigAct-1));
		break;
	    case 9: case 10: case 11: case 12: case 13:
		ch->chVol=ch->chVol+(1<<(ch->chMRetrigAct-9));
		break;
	    case 6:  ch->chVol=(ch->chVol*5)>>3; break;
	    case 14: ch->chVol=(ch->chVol*3)>>1; break;
	    case 7:  ch->chVol>>=1; break;
	    case 15: ch->chVol<<=1; break;
	    }
	    ch->chFinalVol=ch->chVol=volrange(ch->chVol);
	    break;
	case xmpCmdTremor:
	    if (ch->chTremorPos>=ch->chTremorOff)
		ch->chFinalVol=0;
	    if (tick0)
		break;
	    ch->chTremorPos++;
	    if (ch->chTremorPos==ch->chTremorLen)
		ch->chTremorPos=0;
	    break;
	case xmpCmdRetrigger:
	    if (!ch->chActionTick)
		break;
	    if (!(curtick%ch->chActionTick))
		ch->nextpos=0;
	    break;
	case xmpCmdNoteCut:
	    if (tick0)
		break;
	    if (curtick==ch->chActionTick)
		ch->chFinalVol=ch->chVol=0;
	    break;
	case xmpCmdDelayNote:
	    if (tick0)
		break;
	    if (curtick!=ch->chActionTick)
		break;
	    procnot=ch->chDelayNote;
	    procins=ch->chDelayIns;
	    proccmd=0;
	    procdat=0;
	    procvol=0;
	    PlayNote(i);
	    switch (ch->chDelayVol>>4) {
	    case xmpVCmdVol0x: case xmpVCmdVol1x: case xmpVCmdVol2x: case xmpVCmdVol3x:
		ch->chFinalVol=ch->chVol=ch->chDelayVol-0x10;
		break;
	    case xmpVCmdVol40:
		ch->chFinalVol=ch->chVol=0x40;
		break;
	    case xmpVCmdPanning:
		ch->chFinalPan=ch->chPan=(ch->chDelayVol&0xF)*0x11;
		break;
	    }
	    break;
	}

	if (!ch->cursamp) {
	    driver_stopnote(i);
	} else {
	    xmplayer_final_channel_ops(i);
	}
    }
}

void
xmplayer_init_module (void)
{
    g_assert(xm != NULL);

    curtempo = xm->tempo;
    curbpm = xm->bpm;
}

static gboolean
xmplayer_init_playing ()
{
    int i, j;
    st_mixer_sample_info s;

    memset(channels, 0, sizeof(channels));

    driver_setnumch(xm->num_channels);

    for(i = 0; i < 128; i++) {
	for(j = 0; j < 16; j++) {
	    STSample *t = &xm->instruments[i].samples[j];
	    if(t->length != 0) {
		if(t->type == 8)
		    s.type = ST_MIXER_SAMPLE_TYPE_8;
		else
		    s.type = ST_MIXER_SAMPLE_TYPE_16;
		s.length = t->length;
		s.looptype = t->loop;
	        s.loopstart = t->loopstart;
		s.loopend = t->loopend;
		s.data = t->data;
		if(!driver_setsample(i * 16 + j, &s))
		    return FALSE;
	    } else {
		if(!driver_setsample(i * 16 + j, NULL))
		    return FALSE;
	    }
	}
    }

    current_time = 0.0;

    globalvol = 0x40;
    realgvol = 0x40;
    ninst = 128;
    nord = xm->song_length;
    nsamp = 16;
    ismod = xm->flags & XM_FLAGS_IS_MOD;
    linearfreq = !(xm->flags & XM_FLAGS_AMIGA_FREQ);
    nchan = xm->num_channels;
    loopord = xm->restart_position;
    curtick = curtempo-1;
    patdelay = 0;

    return TRUE;
}

gboolean
xmplayer_init_play_song (int songpos,
			 int patpos)
{
    jumptorow = patpos;
    currow = patpos;
    jumptoord = songpos;
    curord = songpos;
    xmplayer_playmode = PLAYING_SONG;

    if(songpos == 0)
	xmplayer_init_module();

    return xmplayer_init_playing();
}

gboolean
xmplayer_init_play_pattern (int pattern,
			    int patpos)
{
    jumptorow = patpos;
    currow = patpos;
    jumptoord = 0;
    curord = 0;
    patlen = xm->patterns[pattern].length;
    curpattern = &xm->patterns[pattern];
    xmplayer_playmode = PLAYING_PATTERN;

    return xmplayer_init_playing();
}

void
xmplayer_stop (void)
{
    xmplayer_playmode = 0;
}

gboolean
xmplayer_play_note (int channel,
		    int note,
		    int instrument)
{
    if(!xmplayer_playmode) {
	xmplayer_playmode = PLAYING_NOTE;
	if(!xmplayer_init_playing())
	    return FALSE;
    }

    /* start note here */
    memset(&channels[channel], 0, sizeof(channels[channel]));

    proccmd = 0;
    procnot = note;
    procins = channels[channel].chCurIns = instrument;
    procdat = 0;
    procvol = 0;

    PlayNote(channel);

    channels[channel].nextsamp = (void*)1;
    channels[channel].nextpos = -1;
    channels[channel].nextstop = 0;
    xmplayer_final_channel_ops(channel);

    return TRUE;
}

double
xmplayer_play (void)
{
    player_songpos = curord;
    player_patpos = currow;

    xmpPlayTick();
    current_time += (double)125 / (curbpm * 50);
    return current_time;
}

void
xmplayer_set_songpos (int songpos)
{
    jumptorow = currow = player_patpos = 0;
    jumptoord = curord = player_songpos = songpos;
    curtick = curtempo - 1;
    globalvol = 64;
}

void
xmplayer_set_pattern (int pattern)
{
    patlen = xm->patterns[pattern].length;
    curpattern = &xm->patterns[pattern];

    if(currow >= patlen) {
	currow = jumptorow = 0;
    }
}
