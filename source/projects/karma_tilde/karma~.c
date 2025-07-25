#include "karma.h"

typedef enum {
    CONTROL_STATE_ZERO = 0,
    CONTROL_STATE_RECORD_INITIAL_LOOP = 1,
    CONTROL_STATE_RECORD_ALTERNATE = 2,
    CONTROL_STATE_RECORD_OFF = 3,
    CONTROL_STATE_PLAY_ALTERNATE = 4,
    CONTROL_STATE_PLAY_ON = 5,
    CONTROL_STATE_STOP_ALTERNATE = 6,
    CONTROL_STATE_STOP = 7,
    CONTROL_STATE_JUMP = 8,
    CONTROL_STATE_APPEND = 9,
    CONTROL_STATE_APPEND_SPECIAL = 10,
    CONTROL_STATE_RECORD_ON = 11,
} control_state;


typedef enum {
    HUMAN_STATE_STOP = 0,
    HUMAN_STATE_PLAY = 1,
    HUMAN_STATE_RECORD = 2,
    HUMAN_STATE_OVERDUB = 3,
    HUMAN_STATE_APPEND = 4,
    HUMAN_STATE_INITIAL = 5,
} human_state;


typedef enum {
    SR_TYPE_LINEAR = 0,
    SR_TYPE_SINE_EASE_IN = 1,
    SR_TYPE_CUBIC_EASE_IN = 2,
    SR_TYPE_CUBIC_EASE_OUT = 3,
    SR_TYPE_EXP_EASE_IN = 4,
    SR_TYPE_EXP_EASE_OUT = 5,
    SR_TYPE_EXP_EASE_INOUT = 6,
} switchramp_type;


typedef enum {
    INTERP_LINEAR = 0,
    INTERP_CUBIC = 1,
    INTERP_SPLINE = 3,
} interp_type;

// clang-format off
struct t_karma {
    
    t_pxobject      k_ob;
    t_buffer_ref    *buf;
    t_buffer_ref    *buf_temp;      // so that 'set' errors etc do not interupt current buf playback ...
    t_symbol        *bufname;
    t_symbol        *bufname_temp;  // ...

    double  ssr;            // system samplerate
    double  bsr;            // buffer samplerate
    double  bmsr;           // buffer samplerate in samples-per-millisecond
    double  srscale;        // scaling factor: buffer samplerate / system samplerate ("to scale playback speeds appropriately")
    double  vs;             // system vectorsize
    double  vsnorm;         // normalised system vectorsize
    double  bvsnorm;        // normalised buffer vectorsize

    double  o1prev;         // previous sample value of "osamp1" etc...
    double  o2prev;         // ...
    double  o3prev;
    double  o4prev;
    double  o1dif;          // (o1dif = o1prev - osamp1) etc...
    double  o2dif;          // ...
    double  o3dif;
    double  o4dif;
    double  writeval1;      // values to be written into buffer~...
    double  writeval2;      // ...after ipoke~ interpolation, overdub summing, etc...
    double  writeval3;      // ...
    double  writeval4;

    double  playhead;       // play position in samples (raja: "double so that capable of tracking playhead position in floating-point indices")
    double  maxhead;        // maximum playhead position that the recording has gone into the buffer~ in samples  // ditto
    double  jumphead;       // jump position (in terms of phase 0..1 of loop) <<-- of 'loop', not 'buffer~'
    double  selstart;       // start position of window ('selection') within loop set by the 'position $1' message sent to object (in phase 0..1)
    double  selection;      // selection length of window ('selection') within loop set by 'window $1' message sent to object (in phase 0..1)
//  double  selmultiply;    // store loop length multiplier amount from 'multiply' method -->> TODO
    double  snrfade;        // fade counter for switch n ramp, normalised 0..1 ??
    double  overdubamp;     // overdub amplitude 0..1 set by 'overdub $1' message sent to object
    double  overdubprev;    // a 'current' overdub amount ("for smoothing overdub amp changes")
    double  speedfloat;     // store speed inlet value if float (not signal)

    long    syncoutlet;     // make sync outlet ? (object attribute @syncout, instantiation time only)
//  long    boffset;        // zero indexed buffer channel # (default 0), user settable, not buffer~ queried -->> TODO
    long    moduloout;      // modulo playback channel outputs flag, user settable, not buffer~ queried -->> TODO
    long    islooped;       // can disable/enable global looping status (rodrigo @ttribute request, TODO) (!! long ??)

    long   bframes;         // number of buffer frames (number of floats long the buffer is for a single channel)
    long   bchans;          // number of buffer channels (number of floats in a frame, stereo has 2 samples per frame, etc.)
    long   ochans;          // number of object audio channels (object arg #2: 1 / 2 / 4)
    long   nchans;          // number of channels to actually address (use only channel one if 'ochans' == 1, etc.)

    interp_type interpflag; // playback interpolation, 0 = linear, 1 = cubic, 2 = spline (!! why is this a long ??)
    long   recordhead;      // record head position in samples
    long   minloop;         // the minimum point in loop so far that has been requested as start point (in samples), is static value
    long   maxloop;         // the overall loop end recorded so far (in samples), is static value
    long   startloop;       // playback start position (in buffer~) in samples, changes depending on loop points and selection logic
    long   endloop;         // playback end position (in buffer~) in samples, changes depending on loop points and selection logic
    long   pokesteps;       // number of steps (samples) to keep track of in ipoke~ linear averaging scheme
    long   recordfade;      // fade counter for recording in samples
    long   playfade;        // fade counter for playback in samples
    long   globalramp;      // general fade time (for both recording and playback) in samples
    long   snrramp;         // switch n ramp time in samples ("generally much shorter than general fade time")
    switchramp_type snrtype;  // switch n ramp curve option choice
    long   reportlist;      // right list outlet report granularity in ms (!! why is this a long ??)
    long   initiallow;      // store inital loop low point after 'initial loop' (default -1 causes default phase 0)
    long   initialhigh;     // store inital loop high point after 'initial loop' (default -1 causes default phase 1)

    short   speedconnect;   // 'count[]' info for 'speed' as signal or float in perform routines

    control_state statecontrol; // master looper state control (not 'human state')
    human_state statehuman; // master looper state human logic (not 'statecontrol') (0=stop, 1=play, 2=record, 3=overdub, 4=append 5=initial)

    char    playfadeflag;   // playback up/down flag, used as: 0 = fade up/in, 1 = fade down/out (<<-- TODO: reverse ??) but case switch 0..4 ??
    char    recfadeflag;    // record up/down flag, 0 = fade up/in, 1 = fade down/out (<<-- TODO: reverse ??) but used 0..5 ??
    char    recendmark;     // the flag to show that the loop is done recording and to mark the ending of it
    char    directionorig;  // original direction loop was recorded ("if loop was initially recorded in reverse started from end-of-buffer etc")
    char    directionprev;  // previous direction ("marker for directional changes to place where fades need to happen during recording")
    
    t_bool  stopallowed;    // flag, 'false' if already stopped once (& init)
    t_bool  go;             // execute play ??
    t_bool  record;         // record flag
    t_bool  recordprev;     // previous record flag
    t_bool  loopdetermine;  // flag: "...for when object is in a recording stage that actually determines loop duration..."
    t_bool  alternateflag;  // ("rectoo") ARGH ?? !! flag that selects between different types of engagement for statecontrol ??
    t_bool  append;         // append flag ??
    t_bool  triginit;       // flag to show trigger start of ...stuff... (?)
    t_bool  wrapflag;       // flag to show if a window selection wraps around the buffer~ end / beginning
    t_bool  jumpflag;       // whether jump is 'on' or 'off' ("flag to block jumps from coming too soon" ??)

    t_bool  recordinit;     // initial record (raja: "...determine whether to apply the 'record' message to initial loop recording or not")
    t_bool  initinit;       // initial initialise (raja: "...hack i used to determine whether DSP is turned on for the very first time or not")
    t_bool  initskip;       // is initialising = 0
    t_bool  buf_modified;   // buffer has been modified bool
    t_bool  clockgo;        // activate clock (for list outlet)

    void    *messout;       // list outlet pointer
    void    *tclock;        // list timer pointer
};


static t_symbol *ps_nothing;
static t_symbol *ps_dummy; 
static t_symbol *ps_buffer_modified;
static t_symbol *ps_phase;
static t_symbol *ps_samples;
static t_symbol *ps_milliseconds;
static t_symbol *ps_originalloop;

static t_class  *karma_class = NULL;


// Linear Interp
static inline double linear_interp(double f, double x, double y)
{
    return (x + f*(y - x));
}

// Hermitic Cubic Interp, 4-point 3rd-order, ( James McCartney / Alex Harker )
static inline double cubic_interp(double f, double w, double x, double y, double z)
{
    return ((((0.5*(z - w) + 1.5*(x - y))*f + (w - 2.5*x + y + y - 0.5*z))*f + (0.5*(y - w)))*f + x);
}

// Catmull-Rom Spline Interp, 4-point 3rd-order, ( Paul Breeuwsma / Paul Bourke )
static inline double spline_interp(double f, double w, double x, double y, double z)
{
    return (((-0.5*w + 1.5*x - 1.5*y + 0.5*z)*pow(f,3)) + ((w - 2.5*x + y + y - 0.5*z)*pow(f,2)) + ((-0.5*w + 0.5*y)*f) + x);
}


// easing function for recording (with ipoke)
static inline double ease_record(double y1, char updwn, double globalramp, long playfade)  // !! rewrite !!
{
    double ifup    = (1.0 - (((double)playfade) / globalramp)) * PI;
    double ifdown  = (((double)playfade) / globalramp) * PI;
    return updwn ? y1 * (0.5 * (1.0 - cos(ifup))) : y1 * (0.5 * (1.0 - cos(ifdown)));
}

// easing function for switch & ramp
static inline double ease_switchramp(double y1, double snrfade, switchramp_type snrtype)
{

    switch (snrtype)
    {
        case SR_TYPE_LINEAR:
            y1  = y1 * (1.0 - snrfade);
            break;
        case SR_TYPE_SINE_EASE_IN:
            y1  = y1 * (1.0 - (sin((snrfade - 1) * PI/2) + 1));
            break;
        case SR_TYPE_CUBIC_EASE_IN:
            y1  = y1 * (1.0 - (snrfade * snrfade * snrfade));
            break;
        case SR_TYPE_CUBIC_EASE_OUT:
            snrfade = snrfade - 1;
            y1  = y1 * (1.0 - (snrfade * snrfade * snrfade + 1));
            break;
        case SR_TYPE_EXP_EASE_IN: 
            snrfade = (snrfade == 0.0) ? snrfade : pow(2, (10 * (snrfade - 1)));
            y1  = y1 * (1.0 - snrfade);
            break;
        case SR_TYPE_EXP_EASE_OUT:
            snrfade = (snrfade == 1.0) ? snrfade : (1 - pow(2, (-10 * snrfade)));
            y1  = y1 * (1.0 - snrfade);
            break;
        case SR_TYPE_EXP_EASE_INOUT:
            if ((snrfade > 0) && (snrfade < 0.5))
                y1 = y1 * (1.0 - (0.5 * pow(2, ((20 * snrfade) - 10))));
            else if ((snrfade < 1) && (snrfade > 0.5))
                y1 = y1 * (1.0 - (-0.5 * pow(2, ((-20 * snrfade) + 10)) + 1));
            break;
    }
    return  y1;
}

// easing function for buffer read
static inline void ease_bufoff(long framesm1, float *buf, long pchans, long markposition, char direction, double globalramp)
{
    long i, fadpos, c;
    double fade;

    if (globalramp <= 0) return;

    for (i = 0; i < globalramp; i++)
    {
        fadpos = markposition + (direction * i);

        if (fadpos < 0 || fadpos > framesm1)
            continue;

        fade = 0.5 * (1.0 - cos(((double)i / globalramp) * PI));

        for (c = 0; c < pchans; c++)
        {
            buf[(fadpos * pchans) + c] *= fade;
        }
    }
}

static inline void apply_fade(long pos, long framesm1, float *buf, long pchans, double fade)
{
    if (pos < 0 || pos > framesm1)
        return;
    for (long c = 0; c < pchans; c++) {
        buf[(pos * pchans) + c] *= fade;
    }
};


// easing function for buffer write
static inline void ease_bufon(long framesm1, float *buf, long pchans, long markposition1, long markposition2, char direction, double globalramp)
{
    long fadpos[3];
    double fade;

    for (long i = 0; i < globalramp; i++)
    {
        fade = 0.5 * (1.0 - cos(((double)i / globalramp) * PI));
        fadpos[0] = (markposition1 - direction) - (direction * i);
        fadpos[1] = (markposition2 - direction) - (direction * i);
        fadpos[2] =  markposition2 + (direction * i);

        apply_fade(fadpos[0], framesm1, buf, pchans, fade);
        apply_fade(fadpos[1], framesm1, buf, pchans, fade);
        apply_fade(fadpos[2], framesm1, buf, pchans, fade);
    }
}

// interpolation points
// Helper to wrap index for forward or reverse looping
static inline long wrap_index(long idx, char directionorig, long maxloop, long framesm1) {
    if (directionorig >= 0) {
        // Forward: wrap between 0 and maxloop
        if (idx < 0)
            return (maxloop + 1) + idx;
        else if (idx > maxloop)
            return idx - (maxloop + 1);
        else
            return idx;
    } else {
        // Reverse: wrap between (framesm1 - maxloop) and framesm1
        long min = framesm1 - maxloop;
        if (idx < min)
            return framesm1 - (min - idx);
        else if (idx > framesm1)
            return min + (idx - framesm1);
        else
            return idx;
    }
}

static inline void interp_index(
    long playhead, long *indx0, long *indx1, long *indx2, long *indx3,
    char direction, char directionorig, long maxloop, long framesm1)
{
    *indx0 = wrap_index(playhead - direction, directionorig, maxloop, framesm1);
    *indx1 = playhead;
    *indx2 = wrap_index(playhead + direction, directionorig, maxloop, framesm1);
    *indx3 = wrap_index(*indx2 + direction, directionorig, maxloop, framesm1);
}


void ext_main(void *r)
{
    t_class *c = class_new("karma~", (method)karma_new, (method)karma_free, (long)sizeof(t_karma), 0L, A_GIMME, 0);

    class_addmethod(c, (method)karma_select_start,  "position", A_FLOAT,    0);
    class_addmethod(c, (method)karma_select_size,   "window",   A_FLOAT,    0);
    class_addmethod(c, (method)karma_jump,          "jump",     A_FLOAT,    0);
    class_addmethod(c, (method)karma_stop,          "stop",                 0);
    class_addmethod(c, (method)karma_play,          "play",                 0);
    class_addmethod(c, (method)karma_record,        "record",               0);
    class_addmethod(c, (method)karma_append,        "append",               0);
    class_addmethod(c, (method)karma_resetloop,     "resetloop",            0);
    class_addmethod(c, (method)karma_setloop,       "setloop",  A_GIMME,    0);
    class_addmethod(c, (method)karma_buf_change,    "set",      A_GIMME,    0);
    class_addmethod(c, (method)karma_overdub,       "overdub",  A_FLOAT,    0);     // !! A_GIMME ?? (for amplitude + smooth time)
    class_addmethod(c, (method)karma_float,         "float",    A_FLOAT,    0);
    
    class_addmethod(c, (method)karma_dsp64,         "dsp64",    A_CANT,     0);
    class_addmethod(c, (method)karma_assist,        "assist",   A_CANT,     0);
    class_addmethod(c, (method)karma_buf_dblclick,  "dblclick", A_CANT,     0);
    class_addmethod(c, (method)karma_buf_notify,    "notify",   A_CANT,     0);

    CLASS_ATTR_LONG(c, "syncout", 0, t_karma, syncoutlet);
    CLASS_ATTR_ACCESSORS(c, "syncout", (method)NULL, (method)karma_syncout_set);    // custom for using at instantiation
    CLASS_ATTR_LABEL(c, "syncout", 0, "Create audio rate Sync Outlet no/yes 0/1");  // not needed anywhere ?
    CLASS_ATTR_INVISIBLE(c, "syncout", 0);                      // do not expose to user, only callable as instantiation attribute

    CLASS_ATTR_LONG(c, "report", 0, t_karma, reportlist);       // !! change to "reporttime" or "listreport" or "listinterval" ??
    CLASS_ATTR_FILTER_MIN(c, "report", 0);
    CLASS_ATTR_LABEL(c, "report", 0, "Report Time (ms) for data outlet");
    
    CLASS_ATTR_LONG(c, "ramp", 0, t_karma, globalramp);         // !! change this to ms input !!    // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "ramp", 0, 2048);
    CLASS_ATTR_LABEL(c, "ramp", 0, "Ramp Time (samples)");
    
    CLASS_ATTR_LONG(c, "snramp", 0, t_karma, snrramp);          // !! change this to ms input !!    // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "snramp", 0, 2048);
    CLASS_ATTR_LABEL(c, "snramp", 0, "Switch&Ramp Time (samples)");
    
    CLASS_ATTR_LONG(c, "snrcurv", 0, t_karma, snrtype);         // !! change to "[something else]" ??
    CLASS_ATTR_FILTER_CLIP(c, "snrcurv", 0, 6);
    CLASS_ATTR_ENUMINDEX(c, "snrcurv", 0, "Linear Sine_In Cubic_In Cubic_Out Exp_In Exp_Out Exp_In_Out");
    CLASS_ATTR_LABEL(c, "snrcurv", 0, "Switch&Ramp Curve");
    
    CLASS_ATTR_LONG(c, "interp", 0, t_karma, interpflag);       // !! change to "playinterp" ??
    CLASS_ATTR_FILTER_CLIP(c, "interp", 0, 2);
    CLASS_ATTR_ENUMINDEX(c, "interp", 0, "Linear Cubic Spline");
    CLASS_ATTR_LABEL(c, "interp", 0, "Playback Interpolation");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    karma_class = c;

    ps_nothing = gensym("");
    ps_dummy = gensym("dummy");
    ps_buffer_modified = gensym("buffer_modified");

    ps_phase = gensym("phase");
    ps_samples = gensym("samples");
    ps_milliseconds = gensym("milliseconds");
    ps_originalloop = gensym("reset");
}
// clang-format on

void* karma_new(t_symbol* s, short argc, t_atom* argv)
{
    t_karma*  x;
    t_symbol* bufname = 0;
    long      syncoutlet = 0;
    long      chans = 0;
    long      attrstart = attr_args_offset(argc, argv);

    x = (t_karma*)object_alloc(karma_class);
    x->initskip = 0;

    // should do better argument checks here
    if (attrstart && argv) {
        bufname = atom_getsym(argv + 0);
        // @arg 0 @name buffer_name @optional 0 @type symbol @digest Name of
        // <o>buffer~</o> to be associated with the <o>karma~</o> instance
        // @description Essential argument: <o>karma~</o> will not operate
        // without an associated <o>buffer~</o> <br /> The associated
        // <o>buffer~</o> determines memory and length (if associating a buffer~
        // of <b>0 ms</b> in size <o>karma~</o> will do nothing) <br /> The
        // associated <o>buffer~</o> can be changed on the fly (see the
        // <m>set</m> message) but one must be present on instantiation <br />
        if (attrstart > 1) {
            chans = atom_getlong(argv + 1);
            if (attrstart > 2) {
                // object_error((t_object *)x, "rodrigo! third arg no longer
                // used! use new @syncout attribute instead!");
                object_warn(
                    (t_object*)x,
                    "too many arguments to karma~, ignoring additional crap");
            }
        }
/*  } else {
        object_error((t_object *)x, "karma~ will not load without an associated buffer~ declaration");
        goto zero;
*/  }

if (x) {
    if (chans <= 1) {
        dsp_setup(
            (t_pxobject*)x,
            2); // one audio channel inlet, one signal speed inlet
        chans = 1;
    } else if (chans == 2) {
        dsp_setup(
            (t_pxobject*)x,
            3); // two audio channel inlets, one signal speed inlet
        chans = 2;
    } else {
        dsp_setup(
            (t_pxobject*)x,
            5); // four audio channel inlets, one signal speed inlet
        chans = 4;
    }

    x->recordhead = -1;
    x->reportlist = 50;                // ms
    x->snrramp = x->globalramp = 256;  // samps...
    x->playfade = x->recordfade = 257; // ...
    x->ssr = sys_getsr();
    x->vs = sys_getblksize();
    x->vsnorm = x->vs / x->ssr;
    x->overdubprev = 1.0;
    x->overdubamp = 1.0;
    x->speedfloat = 1.0;
    x->snrtype = SR_TYPE_SINE_EASE_IN;
    x->interpflag = INTERP_CUBIC;
    x->islooped = 1;
    x->playfadeflag = 0;
    x->recfadeflag = 0;
    x->recordinit = 0;
    x->initinit = 0;
    x->append = 0;
    x->jumpflag = 0;
    x->statecontrol = CONTROL_STATE_ZERO;
    x->statehuman = HUMAN_STATE_STOP;
    x->stopallowed = 0;
    x->go = 0;
    x->triginit = 0;
    x->directionprev = 0;
    x->directionorig = 0;
    x->recordprev = 0;
    x->record = 0;
    x->alternateflag = 0;
    x->recendmark = 0;
    x->pokesteps = 0;
    x->wrapflag = 0;
    x->loopdetermine = 0;
    x->writeval1 = x->writeval2 = x->writeval3 = x->writeval4 = 0;
    x->maxhead = 0.0;
    x->playhead = 0.0;
    x->initiallow = -1;
    x->initialhigh = -1;
    x->selstart = 0.0;
    x->jumphead = 0.0;
    x->snrfade = 0.0;
    x->o1dif = x->o2dif = x->o3dif = x->o4dif = 0.0;
    x->o1prev = x->o2prev = x->o3prev = x->o4prev = 0.0;

    if (bufname != 0)
        x->bufname = bufname; // !! setup is in 'karma_buf_setup()' called by
                              // 'karma_dsp64()'...
    /*      else                        // ...(this means double-clicking karma~
       does not show buffer~ window until DSP turned on, ho hum)
                object_error((t_object *)x, "requires an associated buffer~
       declaration");
    */

    x->ochans = chans;
    // @arg 1 @name num_chans @optional 1 @type int @digest Number of Audio
    // channels
    // @description Default = <b>1 (mono)</b> <br />
    // If <b>1</b>, <o>karma~</o> will operate in mono mode with one input for
    // recording and one output for playback <br /> If <b>2</b>, <o>karma~</o>
    // will operate in stereo mode with two inputs for recording and two outputs
    // for playback <br /> If <b>4</b>, <o>karma~</o> will operate in quad mode
    // with four inputs for recording and four outputs for playback <br />

    x->messout = listout(x); // data
    x->tclock = clock_new((t_object*)x, (method)karma_clock_list);
    attr_args_process(x, argc, argv);
    syncoutlet = x->syncoutlet; // pre-init

    if (chans <= 1) { // mono
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "signal");     // first: audio output
    } else if (chans == 2) {         // stereo
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "signal");     // second: audio output 2
        outlet_new(x, "signal");     // first: audio output 1
    } else {                         // quad
        if (syncoutlet)
            outlet_new(x, "signal"); // last: sync (optional)
        outlet_new(x, "signal");     // fourth: audio output 4
        outlet_new(x, "signal");     // third: audio output 3
        outlet_new(x, "signal");     // second: audio output 2
        outlet_new(x, "signal");     // first: audio output 1
    }

    x->initskip = 1;
    x->k_ob.z_misc |= Z_NO_INPLACE;
}

// zero:
return (x);
}

void karma_free(t_karma* x)
{
    if (x->initskip) {
        dsp_free((t_pxobject*)x);

        object_free(x->buf);
        object_free(x->buf_temp);

        object_free(x->tclock);
        object_free(x->messout);
    }
}

void karma_buf_dblclick(t_karma* x)
{
    buffer_view(buffer_ref_getobject(x->buf));
}

// called by 'karma_dsp64' method
void karma_buf_setup(t_karma* x, t_symbol* s)
{
    t_buffer_obj* buf;
    x->bufname = s;

    if (!x->buf)
        x->buf = buffer_ref_new((t_object*)x, s);
    else
        buffer_ref_set(x->buf, s);

    buf = buffer_ref_getobject(x->buf);

    if (buf == NULL) {
        x->buf = 0;
        // object_error((t_object *)x, "there is no buffer~ named %s",
        // s->s_name);
    } else {
        //  if (buf != NULL) {
        x->directionorig = 0;
        x->maxhead = x->playhead = 0.0;
        x->recordhead = -1;
        x->bchans = buffer_getchannelcount(buf);
        x->bframes = buffer_getframecount(buf);
        x->bmsr = buffer_getmillisamplerate(buf);
        x->bsr = buffer_getsamplerate(buf);
        x->nchans = (x->bchans < x->ochans) ? x->bchans : x->ochans; // MIN
        x->srscale = x->bsr / x->ssr; // x->ssr / x->bsr;
        x->bvsnorm = x->vsnorm * (x->bsr / (double)x->bframes);
        x->minloop = x->startloop = 0.0;
        x->maxloop = x->endloop = (x->bframes - 1); // * ((x->bchans > 1) ?
                                                    // x->bchans : 1);
        x->selstart = 0.0;
        x->selection = 1.0;
    }
}

// called on buffer modified notification
void karma_buf_modify(t_karma* x, t_buffer_obj* b)
{
    double modbsr, modbmsr;
    long   modchans, modframes;

    if (b) {
        modbsr = buffer_getsamplerate(b);
        modchans = buffer_getchannelcount(b);
        modframes = buffer_getframecount(b);
        modbmsr = buffer_getmillisamplerate(b);

        if (((x->bchans != modchans) || (x->bframes != modframes))
            || (x->bmsr != modbmsr)) {
            x->bsr = modbsr;
            x->bmsr = modbmsr;
            x->srscale = modbsr / x->ssr; // x->ssr / modbsr;
            x->bframes = modframes;
            x->bchans = modchans;
            x->nchans = (modchans < x->ochans) ? modchans : x->ochans; // MIN
            x->minloop = x->startloop = 0.0;
            x->maxloop = x->endloop = (x->bframes - 1); // * ((modchans > 1) ?
                                                        // modchans : 1);
            x->bvsnorm = x->vsnorm * (modbsr / (double)modframes);

            karma_select_size(x, x->selection);
            karma_select_start(x, x->selstart);
            //          karma_select_internal(x, x->selstart, x->selection);

            //          post("buff modify called"); // dev
        }
    }
}

// called by 'karma_buf_change_internal' & 'karma_setloop_internal'
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
void karma_buf_values_internal(
    t_karma* x, double templow, double temphigh, long loop_points_flag,
    t_bool caller)
{
    //  t_symbol *loop_points_sym = 0;                      // dev
    t_symbol*     caller_sym = 0;
    t_buffer_obj* buf;
    long          bframesm1;              //, bchanscnt;
                                          //  long bchans;
    double bframesms, bvsnorm, bvsnorm05; // !!
    double low, lowtemp, high, hightemp;
    low = templow;
    high = temphigh;

    if (caller) { // only if called from 'karma_buf_change_internal()'
        buf = buffer_ref_getobject(x->buf);

        x->bchans = buffer_getchannelcount(buf);
        x->bframes = buffer_getframecount(buf);
        x->bmsr = buffer_getmillisamplerate(buf);
        x->bsr = buffer_getsamplerate(buf);
        x->nchans = (x->bchans < x->ochans) ? x->bchans : x->ochans; // MIN
        x->srscale = x->bsr / x->ssr; // x->ssr / x->bsr;

        caller_sym = gensym("set");
    } else {
        caller_sym = gensym("setloop");
    }

    // bchans    = x->bchans;
    bframesm1 = (x->bframes - 1);
    bframesms = (double)bframesm1 / x->bmsr; // buffersize in milliseconds
    bvsnorm = x->vsnorm
        * (x->bsr / (double)x->bframes); // vectorsize in (double) % 0..1
                                         // (phase) units of buffer~
    bvsnorm05 = bvsnorm * 0.5;           // half vectorsize (normalised)
    x->bvsnorm = bvsnorm;

    // by this stage in routine, if LOW < 0., it has not been set and should be
    // set to default (0.) regardless of 'loop_points_flag'
    if (low < 0.)
        low = 0.;

    if (loop_points_flag == 0) { // if PHASE
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1

        // (templow already treated as phase 0..1)
    } else if (loop_points_flag == 1) { // if SAMPLES
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1
        else
            high = high / (double)bframesm1; // normalise samples high 0..1..

        if (low > 0.)
            low = low / (double)bframesm1; // normalise samples low 0..1..
    } else {                               // if MILLISECONDS (default)
        // by this stage in routine, if HIGH < 0., it has not been set and
        // should be set to default (the maximum phase 1.)
        if (high < 0.)
            high = 1.; // already normalised 0..1
        else
            high = high / bframesms; // normalise milliseconds high 0..1..

        if (low > 0.)
            low = low / bframesms; // normalise milliseconds low 0..1..
    }

    // !! treated as normalised 0..1 from here on ... min/max & check & clamp
    // once normalisation has occurred
    lowtemp = low;
    hightemp = high;
    low = MIN(lowtemp, hightemp); // low, high = sort_double(low, high);
    high = MAX(lowtemp, hightemp);

    if (low > 1.) { // already sorted (minmax), so if this is the case we know
                    // we are fucked
        object_warn(
            (t_object*)x,
            "loop minimum cannot be greater than available buffer~ size, "
            "setting to buffer~ size minus vectorsize");
        low = 1. - bvsnorm;
    }
    if (high > 1.) {
        object_warn(
            (t_object*)x,
            "loop maximum cannot be greater than available buffer~ size, "
            "setting to buffer~ size");
        high = 1.;
    }

    // finally check for minimum loop-size ...
    if ((high - low) < bvsnorm) {
        if ((high - low) == 0.) {
            object_warn(
                (t_object*)x, "loop size cannot be zero, ignoring %s command",
                caller_sym);
            return;
        } else {
            object_warn(
                (t_object*)x,
                "loop size cannot be this small, minimum is vectorsize "
                "internally (currently using %.0f samples)",
                x->vs);
            if ((low - bvsnorm05) < 0.) {
                low = 0.;
                high = bvsnorm;
            } else if ((high + bvsnorm05) > 1.) {
                high = 1.;
                low = 1. - bvsnorm;
            } else {
                low = low - bvsnorm05;
                high = high + bvsnorm05;
            }
        }
    }
    // regardless of input choice ('loop_points_flag'), final low/high system is
    // normalised (& clipped) 0..1 (phase)
    low = CLAMP(low, 0., 1.);
    high = CLAMP(high, 0., 1.);
    /*
        // dev
        loop_points_sym = (loop_points_flag > 1) ? ps_milliseconds :
       ((loop_points_flag < 1) ? ps_phase : ps_samples); post("loop start
       normalised %.2f, loop end normalised %.2f, units %s", low, high,
       *loop_points_sym);
        //post("loop start samples %.2f, loop end samples %.2f, units used %s",
       (low * bframesm1), (high * bframesm1), *loop_points_sym);
    */
    // to samples, and account for channels & buffer samplerate
    /*    if (bchans > 1) {
            low     = (low * bframesm1) * bchans;// + channeloffset;
            high    = (high * bframesm1) * bchans;// + channeloffset;
        } else {
            low     = (low * bframesm1);// + channeloffset;
            high    = (high * bframesm1);// + channeloffset;
        }
        x->minloop = x->startloop = low;
        x->maxloop = x->endloop = high;
    */
    x->minloop = x->startloop = low * bframesm1;
    x->maxloop = x->endloop = high * bframesm1;

    // update selection
    karma_select_size(x, x->selection);
    karma_select_start(x, x->selstart);
    // karma_select_internal(x, x->selstart, x->selection);
}

// karma_buf_change method defered
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
void karma_buf_change_internal(
    t_karma* x, t_symbol* s, short argc, t_atom* argv)
{
    // Argument 0: buffer name (already checked to be A_SYM in karma_buf_change)
    t_symbol* b_temp = atom_getsym(argv + 0);
    if (b_temp == ps_nothing) {
        object_error(
            (t_object*)x,
            "%s requires a valid buffer~ declaration (none found)", s->s_name);
        return;
    }

    // Set temporary buffer name for checking
    x->bufname_temp = b_temp;

    // Create or update temporary buffer reference
    if (!x->buf_temp)
        x->buf_temp = buffer_ref_new((t_object*)x, b_temp);
    else
        buffer_ref_set(x->buf_temp, b_temp);

    t_buffer_obj* buf_temp = buffer_ref_getobject(x->buf_temp);

    if (buf_temp == NULL) {
        object_warn(
            (t_object*)x, "cannot find any buffer~ named %s, ignoring",
            b_temp->s_name);
        x->buf_temp = 0;
        object_free(x->buf_temp);
        return;
    }

    // Valid buffer found, clean up temp buffer ref
    x->buf_temp = 0;
    object_free(x->buf_temp);

    // Set main buffer name and reference
    t_symbol* b = atom_getsym(argv + 0);
    x->bufname = b;
    if (!x->buf)
        x->buf = buffer_ref_new((t_object*)x, b);
    else
        buffer_ref_set(x->buf, b);

    // Default loop points and flags
    long      loop_points_flag = 2; // 0 = phase, 1 = samples, 2 = ms (default)
    t_symbol* loop_points_sym = 0;
    double    templow = -1.0, temphigh = -1.0, temphightemp = -1.0;
    t_bool    callerid = true;

    // Reset heads and direction
    x->directionorig = 0;
    x->maxhead = x->playhead = 0.0;
    x->recordhead = -1;

    // Parse loop points type (arg 3)
    if (argc >= 4) {
        t_atom* a3 = argv + 3;
        switch (atom_gettype(a3)) {
        case A_SYM:
            loop_points_sym = atom_getsym(a3);
            if (loop_points_sym == ps_dummy)
                loop_points_flag = 2;
            else if (
                loop_points_sym == ps_phase
                || loop_points_sym == gensym("PHASE")
                || loop_points_sym == gensym("ph"))
                loop_points_flag = 0;
            else if (
                loop_points_sym == ps_samples
                || loop_points_sym == gensym("SAMPLES")
                || loop_points_sym == gensym("samps"))
                loop_points_flag = 1;
            else
                loop_points_flag = 2;
            break;
        case A_LONG:
            loop_points_flag = atom_getlong(a3);
            break;
        case A_FLOAT:
            loop_points_flag = (long)atom_getfloat(a3);
            break;
        default:
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.4, using milliseconds "
                "for args 2 & 3",
                s->s_name);
            loop_points_flag = 2;
            break;
        }
        loop_points_flag = CLAMP(loop_points_flag, 0, 2);
    }

    // Parse loop end (arg 2)
    if (argc >= 3) {
        t_atom* a2 = argv + 2;
        if (atom_gettype(a2) == A_FLOAT) {
            temphigh = atom_getfloat(a2);
            if (temphigh < 0.)
                object_warn(
                    (t_object*)x,
                    "loop maximum cannot be less than 0., resetting");
        } else if (atom_gettype(a2) == A_LONG) {
            temphigh = (double)atom_getlong(a2);
            if (temphigh < 0.)
                object_warn(
                    (t_object*)x,
                    "loop maximum cannot be less than 0., resetting");
        } else if (atom_gettype(a2) == A_SYM && argc < 4) {
            loop_points_sym = atom_getsym(a2);
            if (loop_points_sym == ps_dummy)
                loop_points_flag = 2;
            else if (
                loop_points_sym == ps_phase
                || loop_points_sym == gensym("PHASE")
                || loop_points_sym == gensym("ph"))
                loop_points_flag = 0;
            else if (
                loop_points_sym == ps_samples
                || loop_points_sym == gensym("SAMPLES")
                || loop_points_sym == gensym("samps"))
                loop_points_flag = 1;
            else if (
                loop_points_sym == ps_milliseconds
                || loop_points_sym == gensym("MS")
                || loop_points_sym == gensym("ms"))
                loop_points_flag = 2;
            else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.3, setting to "
                    "milliseconds",
                    s->s_name);
                loop_points_flag = 2;
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.3, setting unit to "
                "maximum",
                s->s_name);
        }
    }

    // Parse loop start (arg 1)
    if (argc >= 2) {
        t_atom* a1 = argv + 1;
        if (atom_gettype(a1) == A_FLOAT) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = atom_getfloat(a1);
                templow = temphightemp;
            } else {
                templow = atom_getfloat(a1);
                if (templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.;
                }
            }
        } else if (atom_gettype(a1) == A_LONG) {
            if (temphigh < 0.) {
                temphightemp = temphigh;
                temphigh = (double)atom_getlong(a1);
                templow = temphightemp;
            } else {
                templow = (double)atom_getlong(a1);
                if (templow < 0.) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.;
                }
            }
        } else if (atom_gettype(a1) == A_SYM) {
            loop_points_sym = atom_getsym(a1);
            if (loop_points_sym == ps_dummy) {
                loop_points_flag = 2;
            } else if (loop_points_sym == ps_originalloop) {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand 'buffername' followed by "
                    "%s message, ignoring",
                    s->s_name, loop_points_sym);
                object_warn(
                    (t_object*)x,
                    "(the %s message cannot be used whilst changing buffer~ "
                    "reference",
                    loop_points_sym);
                object_warn(
                    (t_object*)x,
                    "use %s %s message or just %s message instead)",
                    gensym("setloop"), ps_originalloop, gensym("resetloop"));
                return;
            } else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.2, setting loop "
                    "points to minimum (and maximum)",
                    s->s_name);
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.2, setting loop points "
                "to defaults",
                s->s_name);
        }
    }

    // Call internal function to set buffer values
    karma_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

void karma_buf_change(
    t_karma* x, t_symbol* s, short ac, t_atom* av) // " set ..... "
{
    t_atom store_av[4];
    short  i, j, a;
    a = ac;

    // if error return...

    if (a <= 0) {
        object_error(
            (t_object*)x,
            "%s message must be followed by argument(s) (it does nothing "
            "alone)",
            s->s_name);
        return;
    }

    if (atom_gettype(av + 0) != A_SYM) {
        object_error(
            (t_object*)x,
            "first argument to %s message must be a symbol (associated buffer~ "
            "name)",
            s->s_name);
        return;
    }

    // ...else pass and defer

    if (a > 4) {

        object_warn(
            (t_object*)x,
            "too many arguments for %s message, truncating to first four args",
            s->s_name);
        a = 4;

        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }

    } else {

        for (i = 0; i < a; i++) {
            store_av[i] = av[i];
        }

        for (j = i; j < 4; j++) {
            atom_setsym(store_av + j, ps_dummy);
        }
    }

    defer(x, (method)karma_buf_change_internal, s, ac, store_av); // main method
    // karma_buf_change_internal(x, s, ac, store_av);
}

// Helpers funcs for symbol/unit parsing
t_bool is_phase(t_symbol* sym)
{
    return sym == ps_phase || sym == gensym("PHASE") || sym == gensym("ph");
};

t_bool is_samples(t_symbol* sym)
{
    return sym == ps_samples || sym == gensym("SAMPLES")
        || sym == gensym("samps");
};

t_bool is_ms(t_symbol* sym)
{
    return sym == ps_milliseconds || sym == gensym("MS") || sym == gensym("ms");
};


// karma_setloop method (defered?)
// pete says: i know this proof-of-concept branching is horrible, will rewrite
// soon...
void karma_setloop_internal(t_karma* x, t_symbol* s, short argc, t_atom* argv)
{
    t_bool callerid = false; // identify caller of 'karma_buf_values_internal()'
    t_symbol* loop_points_sym = NULL;
    long      loop_points_flag = 2; // 0 = phase, 1 = samples, 2 = ms (default)
    double    templow = -1.0;
    double    temphigh = -1.0;

    // Helper lambdas for symbol/unit parsing
    // auto is_phase = [](t_symbol *sym) {
    //     return sym == ps_phase || sym == gensym("PHASE") || sym ==
    //     gensym("ph");
    // };
    // auto is_samples = [](t_symbol *sym) {
    //     return sym == ps_samples || sym == gensym("SAMPLES") || sym ==
    //     gensym("samps");
    // };
    // auto is_ms = [](t_symbol *sym) {
    //     return sym == ps_milliseconds || sym == gensym("MS") || sym ==
    //     gensym("ms");
    // };

    // Parse third argument: loop points type
    if (argc >= 3) {
        if (argc > 3) {
            object_warn(
                (t_object*)x,
                "too many arguments for %s message, truncating to first three "
                "args",
                s->s_name);
        }
        t_atom* arg2 = argv + 2;
        switch (atom_gettype(arg2)) {
        case A_SYM:
            loop_points_sym = atom_getsym(arg2);
            if (is_phase(loop_points_sym))
                loop_points_flag = 0;
            else if (is_samples(loop_points_sym))
                loop_points_flag = 1;
            else
                loop_points_flag = 2;
            break;
        case A_LONG:
            loop_points_flag = atom_getlong(arg2);
            break;
        case A_FLOAT:
            loop_points_flag = (long)atom_getfloat(arg2);
            break;
        default:
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.3, using milliseconds "
                "for args 1 & 2",
                s->s_name);
            loop_points_flag = 2;
            break;
        }
        loop_points_flag = CLAMP(loop_points_flag, 0, 2);
    }

    // Parse second argument: loop end
    if (argc >= 2) {
        t_atom* arg1 = argv + 1;
        if (atom_gettype(arg1) == A_FLOAT) {
            temphigh = atom_getfloat(arg1);
            if (temphigh < 0.0) {
                object_warn(
                    (t_object*)x,
                    "loop maximum cannot be less than 0., resetting");
            }
        } else if (atom_gettype(arg1) == A_LONG) {
            temphigh = (double)atom_getlong(arg1);
            if (temphigh < 0.0) {
                object_warn(
                    (t_object*)x,
                    "loop maximum cannot be less than 0., resetting");
            }
        } else if (atom_gettype(arg1) == A_SYM && argc < 3) {
            loop_points_sym = atom_getsym(arg1);
            if (is_phase(loop_points_sym))
                loop_points_flag = 0;
            else if (is_samples(loop_points_sym))
                loop_points_flag = 1;
            else if (is_ms(loop_points_sym))
                loop_points_flag = 2;
            else {
                object_warn(
                    (t_object*)x,
                    "%s message does not understand arg no.2, setting to "
                    "milliseconds",
                    s->s_name);
                loop_points_flag = 2;
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.2, setting unit to "
                "maximum",
                s->s_name);
        }
    }

    // Parse first argument: loop start
    if (argc >= 1) {
        t_atom* arg0 = argv + 0;
        if (atom_gettype(arg0) == A_FLOAT) {
            if (temphigh < 0.0) {
                double temp = temphigh;
                temphigh = atom_getfloat(arg0);
                templow = temp;
            } else {
                templow = atom_getfloat(arg0);
                if (templow < 0.0) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.0;
                }
            }
        } else if (atom_gettype(arg0) == A_LONG) {
            if (temphigh < 0.0) {
                double temp = temphigh;
                temphigh = (double)atom_getlong(arg0);
                templow = temp;
            } else {
                templow = (double)atom_getlong(arg0);
                if (templow < 0.0) {
                    object_warn(
                        (t_object*)x,
                        "loop minimum cannot be less than 0., setting to 0.");
                    templow = 0.0;
                }
            }
        } else {
            object_warn(
                (t_object*)x,
                "%s message does not understand arg no.1, resetting loop point",
                s->s_name);
        }
    }

    karma_buf_values_internal(x, templow, temphigh, loop_points_flag, callerid);
}

void karma_setloop(
    t_karma* x, t_symbol* s, short ac, t_atom* av) // " setloop ..... "
{
    t_symbol* reset_sym = 0;
    long points_flag = 1;    // initial low/high points stored as (long)samples
                             // internally
    bool   callerid = false; // false = called from "setloop"
    double initiallow = (double)x->initiallow;
    double initialhigh = (double)x->initialhigh;

    if (ac == 1) { // " setloop reset " message
        if (atom_gettype(av)
            == A_SYM) { // same as sending " resetloop " message to karma~
            reset_sym = atom_getsym(av);
            if (reset_sym
                == ps_originalloop) { // if "reset" message argument...
                                      // ...go straight to calling function with
                                      // initial loop variables...
                                      //              if (!x->recordinit)
                karma_buf_values_internal(
                    x, initiallow, initialhigh, points_flag, callerid);
                //              else
                //                  return;

            } else {
                object_error(
                    (t_object*)x, "%s does not undertsand message %s, ignoring",
                    s->s_name, reset_sym);
                return;
            }
        } else { // ...else pass onto pre-calling parsing function...
            karma_setloop_internal(x, s, ac, av);
        }
    } else { // ...
             //  defer(x, (method)karma_setloop_internal, s, ac, av);
        karma_setloop_internal(x, s, ac, av);
    }
}

// same as sending " setloop reset " message to karma~
void karma_resetloop(t_karma* x) // " resetloop " message only
{
    long points_flag = 1;    // initial low/high points stored as samples
                             // internally
    bool   callerid = false; // false = called from "resetloop"
    double initiallow = (double)x->initiallow; // initial low/high points stored
                                               // as long...
    double initialhigh = (double)x->initialhigh; // ...

    //  if (!x->recordinit)
    karma_buf_values_internal(
        x, initiallow, initialhigh, points_flag, callerid);
    //  else
    //      return;
}

void karma_clock_list(t_karma* x)
{
    t_bool rlgtz = x->reportlist > 0;

    if (rlgtz) // ('reportlist 0' == off, else milliseconds)
    {
        long frames = x->bframes - 1; // !! no '- 1' ??
        long maxloop = x->maxloop;
        long minloop = x->minloop;
        long setloopsize;

        double bmsr = x->bmsr;
        double playhead = x->playhead;
        double selection = x->selection;
        double normalisedposition;
        setloopsize = maxloop - minloop;

        float reversestart = ((double)(frames - setloopsize));
        float forwardstart = ((double)minloop); // ??           // (minloop + 1)
        float reverseend = ((double)frames);
        float forwardend = ((
            double)maxloop); // !!           // (maxloop + 1)        // !! only
                             // broken on initial buffersize report ?? !!
        float selectionsize
            = (selection * ((double)setloopsize)); // (setloopsize
                                                   // + 1)    //
                                                   // !! only
                                                   // broken on
                                                   // initial
                                                   // buffersize
                                                   // report ??
                                                   // !!

        t_bool directflag = x->directionorig < 0; // !! reverse = 1, forward = 0
        t_bool record = x->record; // pointless (and actually is 'record' or
                                   // 'overdub')
        t_bool go = x->go; // pointless (and actually this is on whenever
                           // transport is,...
                           // ...not stricly just 'play')
        char statehuman = x->statehuman;
        //  ((playhead-(frames-maxloop))/setloopsize) :
        //  ((playhead-startloop)/setloopsize)  // ??
        normalisedposition = CLAMP(
            directflag ? ((playhead - (frames - setloopsize)) / setloopsize)
                       : ((playhead - minloop) / setloopsize),
            0., 1.);

        t_atom datalist[7]; // !! reverse logics are wrong ??
        atom_setfloat(
            datalist + 0, normalisedposition); // position float normalised 0..1
        atom_setlong(datalist + 1, go);        // play flag int
        atom_setlong(datalist + 2, record);    // record flag int
        atom_setfloat(
            datalist + 3,
            (directflag ? reversestart : forwardstart)
                / bmsr); // start float ms
        atom_setfloat(
            datalist + 4,
            (directflag ? reverseend : forwardend) / bmsr);  // end float ms
        atom_setfloat(datalist + 5, (selectionsize / bmsr)); // window float ms
        atom_setlong(datalist + 6, statehuman);              // state flag int

        outlet_list(x->messout, 0L, 7, datalist);
        //      outlet_list(x->messout, gensym("list"), 7, datalist);

        if (sys_getdspstate() && (rlgtz)) { // '&& (x->reportlist > 0)' ??
            clock_delay(x->tclock, x->reportlist);
        }
    }
}

void karma_assist(t_karma* x, void* b, long m, long a, char* s)
{
    long dummy;
    long synclet;
    dummy = a + 1;
    synclet = x->syncoutlet;
    a = (a < x->ochans) ? 0 : ((a > x->ochans) ? 2 : 1);

    if (m == ASSIST_INLET) {
        switch (a) {
        case 0:
            if (dummy == 1) {
                if (x->ochans == 1)
                    strncpy_zero(
                        s, "(signal) Record Input / messages to karma~", 256);
                else
                    strncpy_zero(
                        s, "(signal) Record Input 1 / messages to karma~", 256);
            } else {
                snprintf_zero(s, 256, "(signal) Record Input %ld", dummy);
            }
            break;
        case 1:
            strncpy_zero(
                s, "(signal/float) Speed Factor (1. == normal speed)", 256);
            break;
        case 2:
            break;
        }
    } else { // ASSIST_OUTLET
        switch (a) {
        case 0:
            if (x->ochans == 1)
                strncpy_zero(s, "(signal) Audio Output", 256);
            else
                snprintf_zero(s, 256, "(signal) Audio Output %ld", dummy);
            break;
        case 1:
            if (synclet)
                strncpy_zero(
                    s, "(signal) Sync Outlet (current position 0..1)", 256);
            else
                strncpy_zero(
                    s,
                    "List: current position (float 0..1) play state (int 0/1) "
                    "record state (int 0/1) start position (float ms) end "
                    "position (float ms) window size (float ms) current state "
                    "(int 0=stop 1=play 2=record 3=overdub 4=append 5=initial)",
                    512);
            break;
        case 2:
            strncpy_zero(
                s,
                "List: current position (float 0..1) play state (int 0/1) "
                "record state (int 0/1) start position (float ms) end position "
                "(float ms) window size (float ms) current state (int 0=stop "
                "1=play 2=record 3=overdub 4=append 5=initial)",
                512);
            break;
        }
    }
}

//  //  //

void karma_float(t_karma* x, double speedfloat)
{
    long inlet = proxy_getinlet((t_object*)x);
    long chans = (long)x->ochans;

    if (inlet == chans) { // if speed inlet
        x->speedfloat = speedfloat;
    }
}


void karma_select_start(
    t_karma* x,
    double   positionstart) // positionstart = "position" float message
{
    long bfrmaesminusone, setloopsize;
    x->selstart = CLAMP(positionstart, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->loopdetermine) {
        setloopsize = x->maxloop - x->minloop;

        if (x->directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->bframes - 1;

            x->startloop = CLAMP(
                (bfrmaesminusone - x->maxloop) + (positionstart * setloopsize),
                bfrmaesminusone - x->maxloop, bfrmaesminusone);
            x->endloop = x->startloop + (x->selection * x->maxloop);

            if (x->endloop > bfrmaesminusone) {
                x->endloop = (bfrmaesminusone - setloopsize)
                    + (x->endloop - bfrmaesminusone);
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            x->startloop = CLAMP(
                ((positionstart * setloopsize) + x->minloop), x->minloop,
                x->maxloop); // no need for CLAMP ??
            x->endloop = x->startloop + (x->selection * setloopsize);

            if (x->endloop > x->maxloop) {
                x->endloop = x->endloop - setloopsize;
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

void karma_select_size(
    t_karma* x, double duration) // duration = "window" float message
{
    long bfrmaesminusone, setloopsize;

    // double minsampsnorm = x->bvsnorm * 0.5;           // half vectorsize
    // samples minimum as normalised value  // !! buffer sr !! x->selection =
    // (duration < 0.0) ? 0.0 : duration; // !! allow zero for rodrigo !!
    x->selection = CLAMP(duration, 0., 1.);

    // for dealing with selection-out-of-bounds logic:

    if (!x->loopdetermine) {
        setloopsize = x->maxloop - x->minloop;
        x->endloop = x->startloop + (x->selection * setloopsize);

        if (x->directionorig < 0) // if originally in reverse
        {
            bfrmaesminusone = x->bframes - 1;

            if (x->endloop > bfrmaesminusone) {
                x->endloop = (bfrmaesminusone - setloopsize)
                    + (x->endloop - bfrmaesminusone);
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0; // selection-in-bounds
            }

        } else { // if originally forwards

            if (x->endloop > x->maxloop) {
                x->endloop = x->endloop - setloopsize;
                x->wrapflag = 1;
            } else {
                x->wrapflag = 0; // selection-in-bounds
            }
        }
    }
}

void karma_stop(t_karma* x)
{
    if (x->initinit) {
        if (x->stopallowed) {
            x->statecontrol = x->alternateflag ? CONTROL_STATE_STOP_ALTERNATE
                                               : CONTROL_STATE_STOP;
            x->append = 0;
            x->statehuman = HUMAN_STATE_STOP;
            x->stopallowed = 0;
        }
    }
}

void karma_play(t_karma* x)
{
    if ((!x->go) && (x->append)) {
        x->statecontrol = CONTROL_STATE_APPEND;
        x->snrfade = 0.0; // !! should disable ??
    } else if ((x->record) || (x->append)) {
        x->statecontrol = x->alternateflag ? CONTROL_STATE_PLAY_ALTERNATE
                                           : CONTROL_STATE_PLAY_ON;
    } else {
        x->statecontrol = CONTROL_STATE_PLAY_ON;
    }

    x->go = 1;
    x->statehuman = HUMAN_STATE_STOP;
    x->stopallowed = 1;
}

// Helper to clear buffer
t_bool _clear_buffer(t_buffer_obj* buf, long bframes, long rchans)
{
    float* b = buffer_locksamples(buf);
    if (!b)
        return 0;
    for (long i = 0; i < bframes; i++) {
        for (long c = 0; c < rchans; c++) {
            b[i * rchans + c] = 0.0f;
        }
    }
    buffer_setdirty(buf);
    buffer_unlocksamples(buf);
    return 1;
};

void karma_record(t_karma* x)
{
    // // Helper to clear buffer
    // auto clear_buffer = [](t_buffer_obj *buf, long bframes, long rchans) ->
    // bool {
    //     float *b = buffer_locksamples(buf);
    //     if (!b)
    //         return false;
    //     for (long i = 0; i < bframes; i++) {
    //         for (long c = 0; c < rchans; c++) {
    //             b[i * rchans + c] = 0.0f;
    //         }
    //     }
    //     buffer_setdirty(buf);
    //     buffer_unlocksamples(buf);
    //     return true;
    // };

    t_buffer_obj* buf = buffer_ref_getobject(x->buf);
    control_state sc = CONTROL_STATE_ZERO;
    human_state   sh = x->statehuman;
    t_bool        record = x->record;
    t_bool        go = x->go;
    t_bool        altflag = x->alternateflag;
    t_bool        append = x->append;
    t_bool        init = x->recordinit;

    x->stopallowed = 1;

    if (record) {
        if (altflag) {
            sc = CONTROL_STATE_RECORD_ALTERNATE;
            sh = HUMAN_STATE_OVERDUB;
        } else {
            sc = CONTROL_STATE_RECORD_OFF;
            sh = (sh == HUMAN_STATE_OVERDUB) ? HUMAN_STATE_PLAY
                                             : HUMAN_STATE_RECORD;
        }
    } else if (append) {
        if (go) {
            if (altflag) {
                sc = CONTROL_STATE_RECORD_ALTERNATE;
                sh = HUMAN_STATE_OVERDUB;
            } else {
                sc = CONTROL_STATE_APPEND_SPECIAL;
                sh = HUMAN_STATE_APPEND;
            }
        } else {
            sc = CONTROL_STATE_RECORD_INITIAL_LOOP;
            sh = HUMAN_STATE_INITIAL;
        }
    } else if (!go) {
        init = 1;
        if (buf) {
            long rchans = x->bchans;
            long bframes = x->bframes;
            _clear_buffer(buf, bframes, rchans);
        }
        sc = CONTROL_STATE_RECORD_INITIAL_LOOP;
        sh = HUMAN_STATE_INITIAL;
    } else {
        sc = CONTROL_STATE_RECORD_ON;
        sh = HUMAN_STATE_OVERDUB;
    }

    x->go = 1;
    x->recordinit = init;
    x->statecontrol = sc;
    x->statehuman = sh;
}


void karma_append(t_karma* x)
{
    if (x->recordinit) {
        if ((!x->append) && (!x->loopdetermine)) {
            x->append = 1;
            x->maxloop = (x->bframes - 1);
            x->statecontrol = CONTROL_STATE_APPEND;
            x->statehuman = HUMAN_STATE_APPEND;
            x->stopallowed = 1;
        } else {
            object_error(
                (t_object*)x,
                "can't append if already appending, or during 'initial-loop', "
                "or if buffer~ is full");
        }
    } else {
        object_error(
            (t_object*)x,
            "warning! no 'append' registered until at least one loop has been "
            "created first");
    }
}

void karma_overdub(t_karma* x, double amplitude)
{
    x->overdubamp = CLAMP(amplitude, 0.0, 1.0);
}


void karma_jump(t_karma* x, double jumpposition)
{
    if (x->initinit) {
        if (!((x->loopdetermine) && (!x->record))) {
            x->statecontrol = CONTROL_STATE_JUMP;
            x->jumphead = CLAMP(
                jumpposition, 0.,
                1.); // for now phase only, TODO - ms & samples
            //          x->statehuman = HUMAN_STATE_PLAY;           // no -
            //          'jump' is whatever 'statehuman' currently is (most
            //          likely 'play')
            x->stopallowed = 1;
        }
    }
}


t_max_err karma_syncout_set(t_karma* x, t_object* attr, long argc, t_atom* argv)
{
    long syncout = atom_getlong(argv);

    if (!x->initskip) {
        if (argc && argv) {
            x->syncoutlet = CLAMP(syncout, 0, 1);
        }
    } else {
        object_warn(
            (t_object*)x,
            "the syncout attribute is only available at instantiation time, "
            "ignoring 'syncout %ld'",
            syncout);
    }

    return 0;
}


t_max_err
karma_buf_notify(t_karma* x, t_symbol* s, t_symbol* msg, void* sndr, void* dat)
{
    //  t_symbol *bufnamecheck = (t_symbol *)object_method((t_object *)sndr,
    //  gensym("getname"));

    //  if (bufnamecheck == x->bufname) {   // check...
    if (buffer_ref_exists(x->buf)) { // this hack does not really work...
        if (msg == ps_buffer_modified)
            x->buf_modified = true;                          // set flag
        return buffer_ref_notify(x->buf, s, msg, sndr, dat); // ...return
    } else {
        return MAX_ERR_NONE;
    }
}

void karma_dsp64(
    t_karma* x, t_object* dsp64, short* count, double srate, long vecount,
    long flags)
{
    x->ssr = srate;
    x->vs = (double)vecount;
    x->vsnorm = (double)vecount / srate; // x->vs / x->ssr;
    x->clockgo = 1;

    if (x->bufname != 0) {

        if (!x->initinit)
            karma_buf_setup(x, x->bufname); // does 'x->bvsnorm'    // !! this
                                            // should be defered ??

        x->speedconnect = count[1]; // speed is 2nd inlet
        object_method(
            dsp64, gensym("dsp_add64"), x, karma_mono_perform, 0, NULL);

        if (!x->initinit) {
            karma_select_size(x, 1.);
            x->initinit = 1;
        } else {
            karma_select_size(x, x->selection);
            karma_select_start(x, x->selstart);
            //          karma_select_internal(x, x->selstart, x->selection);
        }
    }
}


//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //
//  //  //  //  //  //  //  //  (crazy) PERFORM ROUTINES    //  //  //  //  //
//  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //


// Helper function implementations

static void karma_perform_state_control(
    t_karma* x, control_state* statecontrol, t_bool* record, t_bool* go,
    t_bool* triginit, t_bool* loopdetermine, t_bool* alternateflag,
    char* recendmark, long* recordfade, long* playfade, char* recfadeflag,
    char* playfadeflag, double* snrfade)
{
    switch (*statecontrol) {
    case CONTROL_STATE_ZERO:
        break;
    case CONTROL_STATE_RECORD_INITIAL_LOOP:
        *record = *go = *triginit = *loopdetermine = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *recfadeflag = *playfade = *playfadeflag = 0;
        break;
    case CONTROL_STATE_RECORD_ALTERNATE:
        *recendmark = 3;
        *record = *recfadeflag = *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_RECORD_OFF:
        *recfadeflag = 1;
        *playfadeflag = 3;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_PLAY_ALTERNATE:
        *recendmark = 2;
        *recfadeflag = *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        *playfade = *recordfade = 0;
        break;
    case CONTROL_STATE_PLAY_ON:
        *triginit = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_STOP_ALTERNATE:
        *playfade = *recordfade = 0;
        *recendmark = *playfadeflag = *recfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_STOP:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 1;
        }
        *playfade = 0;
        *playfadeflag = 1;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_JUMP:
        if (*record) {
            *recordfade = 0;
            *recfadeflag = 2;
        }
        *playfade = 0;
        *playfadeflag = 2;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_APPEND:
        *playfadeflag = 4;
        *playfade = 0;
        *statecontrol = CONTROL_STATE_ZERO;
        break;
    case CONTROL_STATE_APPEND_SPECIAL:
        *record = *loopdetermine = *alternateflag = 1;
        *snrfade = 0.0;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *recfadeflag = 0;
        break;
    case CONTROL_STATE_RECORD_ON:
        *playfadeflag = 3;
        *recfadeflag = 5;
        *statecontrol = CONTROL_STATE_ZERO;
        *recordfade = *playfade = 0;
        break;
    }
}

static void karma_perform_direction_change(
    t_karma* x, char direction, char directionprev, t_bool record,
    double globalramp, float* b, long frames, long pchans, long recordhead,
    double* snrfade, long* recordfade, char* recfadeflag, long* recordhead_ptr)
{
    if (directionprev != direction) {
        if (record && globalramp) {
            ease_bufoff(
                frames - 1, b, pchans, recordhead, -direction, globalramp);
            *recordfade = *recfadeflag = 0;
            *recordhead_ptr = -1;
        }
        *snrfade = 0.0;
    }
}

static void karma_perform_record_state_change(
    t_karma* x, t_bool record, t_bool recordprev, double globalramp, float* b,
    long frames, long pchans, long recordhead, double accuratehead,
    char direction, double speed, double* snrfade, long* recordfade,
    char* recfadeflag, long* recordhead_ptr, t_bool* dirt)
{
    if ((record - recordprev) < 0) {
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, recordhead, direction, globalramp);
        *recordhead_ptr = -1;
        *dirt = 1;
    } else if ((record - recordprev) > 0) {
        *recordfade = *recfadeflag = 0;
        if (speed < 1.0)
            *snrfade = 0.0;
        if (globalramp)
            ease_bufoff(
                frames - 1, b, pchans, accuratehead, -direction, globalramp);
    }
    recordprev = record;
}

static double karma_perform_interpolation(
    t_karma* x, double accuratehead, char direction, char directionorig,
    long maxloop, long frames, float* b, long pchans, interp_type interp,
    long* playhead_ptr, long* interp0_ptr, long* interp1_ptr, long* interp2_ptr,
    long* interp3_ptr)
{
    double frac;
    double osamp1;

    *playhead_ptr = trunc(accuratehead);
    if (direction > 0) {
        frac = accuratehead - *playhead_ptr;
    } else if (direction < 0) {
        frac = 1.0 - (accuratehead - *playhead_ptr);
    } else {
        frac = 0.0;
    }

    interp_index(
        *playhead_ptr, interp0_ptr, interp1_ptr, interp2_ptr, interp3_ptr,
        direction, directionorig, maxloop, frames - 1);

    if (interp == INTERP_CUBIC)
        osamp1 = cubic_interp(
            frac, b[*interp0_ptr * pchans], b[*interp1_ptr * pchans],
            b[*interp2_ptr * pchans], b[*interp3_ptr * pchans]);
    else if (interp == INTERP_SPLINE)
        osamp1 = spline_interp(
            frac, b[*interp0_ptr * pchans], b[*interp1_ptr * pchans],
            b[*interp2_ptr * pchans], b[*interp3_ptr * pchans]);
    else
        osamp1 = linear_interp(
            frac, b[*interp1_ptr * pchans], b[*interp2_ptr * pchans]);

    return osamp1;
}

static void karma_perform_playback_processing(
    t_karma* x, double osamp1, double globalramp, double snrramp,
    switchramp_type snrtype, double* snrfade, long* playfade, char playfadeflag,
    t_bool go, t_bool record, t_bool triginit, t_bool jumpflag,
    t_bool loopdetermine, double* o1dif, double o1prev, double* osamp1_ptr,
    t_bool* go_ptr, t_bool* triginit_ptr, t_bool* jumpflag_ptr,
    t_bool* loopdetermine_ptr, double* snrfade_ptr, long* playfade_ptr)
{
    if (globalramp) {
        if (*snrfade < 1.0) {
            if (*snrfade == 0.0) {
                *o1dif = o1prev - osamp1;
            }
            *osamp1_ptr = osamp1 + ease_switchramp(*o1dif, *snrfade, snrtype);
            *snrfade += 1 / snrramp;
        }

        if (*playfade < globalramp) {
            *osamp1_ptr = ease_record(
                *osamp1_ptr, (playfadeflag > 0), globalramp, *playfade);
            (*playfade)++;
            if (*playfade >= globalramp) {
                switch (playfadeflag) {
                case 0:
                    break;
                case 1:
                    playfadeflag = *go_ptr = 0;
                    break;
                case 2:
                    if (!record)
                        *triginit_ptr = *jumpflag_ptr = 1;
                case 3:
                    playfadeflag = *playfade_ptr = 0;
                    break;
                case 4:
                    *go_ptr = *triginit_ptr = *loopdetermine_ptr = 1;
                    *snrfade_ptr = 0.0;
                    *playfade_ptr = 0;
                    playfadeflag = 0;
                    break;
                }
            }
        }
    } else {
        switch (playfadeflag) {
        case 0:
            break;
        case 1:
            playfadeflag = *go_ptr = 0;
            break;
        case 2:
            if (!record)
                *triginit_ptr = *jumpflag_ptr = 1;
        case 3:
            playfadeflag = 0;
            break;
        case 4:
            *go_ptr = *triginit_ptr = *loopdetermine_ptr = 1;
            *snrfade_ptr = 0.0;
            *playfade_ptr = 0;
            playfadeflag = 0;
            break;
        }
    }
}

static void karma_perform_recording(
    t_karma* x, double recin1, t_bool record, double globalramp, float* b,
    long frames, long pchans, long playhead, double overdubamp,
    char recfadeflag, long recordfade, long recordhead, double writeval1,
    double pokesteps, double* recin1_ptr, long* recordhead_ptr,
    double* writeval1_ptr, double* pokesteps_ptr, t_bool* dirt)
{
    long   i;
    double recplaydif, coeff1;

    if (record) {
        if ((recordfade < globalramp) && (globalramp > 0.0))
            *recin1_ptr = ease_record(
                recin1 + (((double)b[playhead * pchans]) * overdubamp),
                recfadeflag, globalramp, recordfade);
        else
            *recin1_ptr = recin1 + ((double)b[playhead * pchans]) * overdubamp;

        if (*recordhead_ptr < 0) {
            *recordhead_ptr = playhead;
            *pokesteps_ptr = 0.0;
        }

        if (*recordhead_ptr == playhead) {
            *writeval1_ptr += *recin1_ptr;
            *pokesteps_ptr += 1.0;
        } else {
            if (*pokesteps_ptr > 1.0) {
                *writeval1_ptr = *writeval1_ptr / *pokesteps_ptr;
                *pokesteps_ptr = 1.0;
            }
            b[*recordhead_ptr * pchans] = *writeval1_ptr;
            recplaydif = (double)(playhead - *recordhead_ptr);
            if (recplaydif > 0) {
                coeff1 = (*recin1_ptr - *writeval1_ptr) / recplaydif;
                for (i = *recordhead_ptr + 1; i < playhead; i++) {
                    *writeval1_ptr += coeff1;
                    b[i * pchans] = *writeval1_ptr;
                }
            } else {
                coeff1 = (*recin1_ptr - *writeval1_ptr) / recplaydif;
                for (i = *recordhead_ptr - 1; i > playhead; i--) {
                    *writeval1_ptr -= coeff1;
                    b[i * pchans] = *writeval1_ptr;
                }
            }
            *writeval1_ptr = *recin1_ptr;
        }
        *recordhead_ptr = playhead;
        *dirt = 1;
    }
}

static void karma_perform_record_fade_processing(
    t_karma* x, double globalramp, long* recordfade, char recfadeflag,
    t_bool record, t_bool triginit, t_bool jumpflag, char recendmark,
    t_bool loopdetermine, char directionorig, long frames, double maxhead,
    t_bool* record_ptr, t_bool* triginit_ptr, t_bool* jumpflag_ptr,
    t_bool* loopdetermine_ptr, char* recendmark_ptr, long* maxloop_ptr)
{
    if (globalramp) {
        if (*recordfade < globalramp) {
            (*recordfade)++;
            if ((recfadeflag) && (*recordfade >= globalramp)) {
                if (recfadeflag == 2) {
                    *recendmark_ptr = 4;
                    *triginit_ptr = *jumpflag_ptr = 1;
                    *recordfade = 0;
                } else if (recfadeflag == 5) {
                    *record_ptr = 1;
                } else {
                    *record_ptr = 0;
                }
                recfadeflag = 0;

                switch (*recendmark_ptr) {
                case 0:
                    *record_ptr = 0;
                    break;
                case 1:
                    if (directionorig < 0) {
                        *maxloop_ptr = (frames - 1) - maxhead;
                    } else {
                        *maxloop_ptr = maxhead;
                    }
                case 2:
                    *record_ptr = *loopdetermine_ptr = 0;
                    *triginit_ptr = 1;
                    break;
                case 3:
                    *record_ptr = *triginit_ptr = 1;
                    *recordfade = *loopdetermine_ptr = 0;
                    break;
                case 4:
                    *recendmark_ptr = 0;
                    break;
                }
            }
        }
    } else {
        if (recfadeflag) {
            if (recfadeflag == 2) {
                *recendmark_ptr = 4;
                *triginit_ptr = *jumpflag_ptr = 1;
            } else if (recfadeflag == 5) {
                *record_ptr = 1;
            } else {
                *record_ptr = 0;
            }
            recfadeflag = 0;

            switch (*recendmark_ptr) {
            case 0:
                *record_ptr = 0;
                break;
            case 1:
                if (directionorig < 0) {
                    *maxloop_ptr = (frames - 1) - maxhead;
                } else {
                    *maxloop_ptr = maxhead;
                }
            case 2:
                *record_ptr = *loopdetermine_ptr = 0;
                *triginit_ptr = 1;
                break;
            case 3:
                *record_ptr = *triginit_ptr = 1;
                *loopdetermine_ptr = 0;
                break;
            case 4:
                *recendmark_ptr = 0;
                break;
            }
        }
    }
}

static void karma_perform_cleanup(
    t_karma* x, t_buffer_obj* buf, t_bool dirt, t_bool go, double o1prev,
    double o1dif, double writeval1, double maxhead, double pokesteps,
    t_bool wrapflag, double snrfade, double accuratehead, char directionorig,
    char directionprev, long recordhead, t_bool alternateflag, long recordfade,
    t_bool triginit, t_bool jumpflag, t_bool go_state, t_bool record,
    t_bool recordprev, control_state statecontrol, char playfadeflag,
    char recfadeflag, long playfade, long minloop, long maxloop,
    long initiallow, long initialhigh, t_bool loopdetermine, long startloop,
    long endloop, double overdubamp, char recendmark, t_bool append)
{
    if (dirt) {
        buffer_setdirty(buf);
    }
    buffer_unlocksamples(buf);

    if (x->clockgo) {
        clock_delay(x->tclock, 0);
        x->clockgo = 0;
    } else if ((!go) || (x->reportlist <= 0)) {
        clock_unset(x->tclock);
        x->clockgo = 1;
    }

    x->o1prev = o1prev;
    x->o1dif = o1dif;
    x->writeval1 = writeval1;
    x->maxhead = maxhead;
    x->pokesteps = pokesteps;
    x->wrapflag = wrapflag;
    x->snrfade = snrfade;
    x->playhead = accuratehead;
    x->directionorig = directionorig;
    x->directionprev = directionprev;
    x->recordhead = recordhead;
    x->alternateflag = alternateflag;
    x->recordfade = recordfade;
    x->triginit = triginit;
    x->jumpflag = jumpflag;
    x->go = go_state;
    x->record = record;
    x->recordprev = recordprev;
    x->statecontrol = statecontrol;
    x->playfadeflag = playfadeflag;
    x->recfadeflag = recfadeflag;
    x->playfade = playfade;
    x->minloop = minloop;
    x->maxloop = maxloop;
    x->initiallow = initiallow;
    x->initialhigh = initialhigh;
    x->loopdetermine = loopdetermine;
    x->startloop = startloop;
    x->endloop = endloop;
    x->overdubprev = overdubamp;
    x->recendmark = recendmark;
    x->append = append;
}

// mono perform

// Helper function to handle state initialization and loading
static void karma_perform_init_state(
    t_karma* x, t_buffer_obj* buf, float** b_ptr, t_bool* record,
    t_bool* recordprev, t_bool* dirt, double* o1prev, double* o1dif,
    double* writeval1, t_bool* go, control_state* statecontrol,
    char* playfadeflag, char* recfadeflag, long* recordhead,
    t_bool* alternateflag, long* pchans, double* srscale, long* frames,
    t_bool* triginit, t_bool* jumpflag, t_bool* append, char* directionorig,
    char* directionprev, long* minloop, long* maxloop, long* initiallow,
    long* initialhigh, double* selection, t_bool* loopdetermine,
    long* startloop, double* selstart, long* endloop, char* recendmark,
    double* overdubamp, double* overdubprev, double* ovdbdif, long* recordfade,
    long* playfade, double* accuratehead, long* playhead, double* maxhead,
    t_bool* wrapflag, double* jumphead, double* pokesteps, double* snrfade,
    double* globalramp, double* snrramp, switchramp_type* snrtype,
    interp_type* interp, double* speedfloat)
{
    *record = x->record;
    *recordprev = x->recordprev;
    *dirt = 0;

    if (!*b_ptr || x->k_ob.z_disabled)
        return;

    if (*record || *recordprev)
        *dirt = 1;

    if (x->buf_modified) {
        karma_buf_modify(x, buf);
        x->buf_modified = false;
    }

    // Load state from object
    *o1prev = x->o1prev;
    *o1dif = x->o1dif;
    *writeval1 = x->writeval1;
    *go = x->go;
    *statecontrol = x->statecontrol;
    *playfadeflag = x->playfadeflag;
    *recfadeflag = x->recfadeflag;
    *recordhead = x->recordhead;
    *alternateflag = x->alternateflag;
    *pchans = x->bchans;
    *srscale = x->srscale;
    *frames = x->bframes;
    *triginit = x->triginit;
    *jumpflag = x->jumpflag;
    *append = x->append;
    *directionorig = x->directionorig;
    *directionprev = x->directionprev;
    *minloop = x->minloop;
    *maxloop = x->maxloop;
    *initiallow = x->initiallow;
    *initialhigh = x->initialhigh;
    *selection = x->selection;
    *loopdetermine = x->loopdetermine;
    *startloop = x->startloop;
    *selstart = x->selstart;
    *endloop = x->endloop;
    *recendmark = x->recendmark;
    *overdubamp = x->overdubprev;
    *overdubprev = x->overdubamp;
    *ovdbdif = (*overdubamp != *overdubprev)
        ? ((*overdubprev - *overdubamp) / 64)
        : 0.0;
    *recordfade = x->recordfade;
    *playfade = x->playfade;
    *accuratehead = x->playhead;
    *playhead = trunc(*accuratehead);
    *maxhead = x->maxhead;
    *wrapflag = x->wrapflag;
    *jumphead = x->jumphead;
    *pokesteps = x->pokesteps;
    *snrfade = x->snrfade;
    *globalramp = (double)x->globalramp;
    *snrramp = (double)x->snrramp;
    *snrtype = x->snrtype;
    *interp = x->interpflag;
    *speedfloat = x->speedfloat;
}

// Helper function to handle loop playback processing
static void karma_perform_loop_playback(
    t_karma* x, t_bool go, t_bool loopdetermine, double* accuratehead,
    double speed, double srscale, long maxloop, long minloop, long frames,
    char direction, char directionorig, double* maxhead, t_bool append,
    char* recendmark, t_bool* triginit, t_bool* alternateflag,
    t_bool* loopdetermine_ptr, double* osamp1)
{
    if (!loopdetermine) {
        // Normal loop playback
        if (go) {
            long   setloopsize = maxloop - minloop;
            double speedsrscaled = speed * srscale;
            *accuratehead = *accuratehead + speedsrscaled;

            // Basic boundary checking
            if (directionorig >= 0) {
                if (*accuratehead > maxloop) {
                    *accuratehead = *accuratehead - setloopsize;
                } else if (*accuratehead < 0.0) {
                    *accuratehead = maxloop + *accuratehead;
                }
            } else {
                if (*accuratehead > (frames - 1)) {
                    *accuratehead = ((frames - 1) - setloopsize)
                        + (*accuratehead - (frames - 1));
                } else if (*accuratehead < ((frames - 1) - maxloop)) {
                    *accuratehead = (frames - 1)
                        - (((frames - 1) - setloopsize) - *accuratehead);
                }
            }
        } else {
            *osamp1 = 0.0;
        }
    } else {
        // Initial loop creation
        if (go) {
            long   setloopsize = maxloop - minloop;
            double speedsrscaled = speed * srscale;
            *accuratehead = *accuratehead + speedsrscaled;

            // Track maximum distance traversed
            if (direction == directionorig) {
                if (*accuratehead > (frames - 1)) {
                    *accuratehead = 0.0;
                    *recendmark = *triginit = 1;
                    *loopdetermine_ptr = *alternateflag = 0;
                    *maxhead = frames - 1;
                } else if (*accuratehead < 0.0) {
                    *accuratehead = frames - 1;
                    *recendmark = *triginit = 1;
                    *loopdetermine_ptr = *alternateflag = 0;
                    *maxhead = 0.0;
                } else {
                    if (((directionorig >= 0) && (*maxhead < *accuratehead))
                        || ((directionorig < 0)
                            && (*maxhead > *accuratehead))) {
                        *maxhead = *accuratehead;
                    }
                }
            }
        }
        *osamp1 = 0.0;
    }
}

// Helper function to handle output generation
static void karma_perform_output_generation(
    t_karma* x, double osamp1, double* o1prev, double** out1, long syncoutlet,
    double** outPh, double accuratehead, long maxloop, long minloop,
    long frames, char directionorig)
{
    *o1prev = osamp1;
    **out1 = osamp1;
    (*out1)++;

    if (syncoutlet) {
        long setloopsize = maxloop - minloop;
        **outPh = (directionorig >= 0)
            ? ((accuratehead - minloop) / setloopsize)
            : ((accuratehead - (frames - setloopsize)) / setloopsize);
        (*outPh)++;
    }
}

// Helper function to handle state updates
static void karma_perform_state_update(
    t_karma* x, t_buffer_obj* buf, t_bool dirt, double o1prev, double o1dif,
    double writeval1, double maxhead, double pokesteps, t_bool wrapflag,
    double snrfade, double accuratehead, char directionorig, char directionprev,
    long recordhead, t_bool alternateflag, long recordfade, t_bool triginit,
    t_bool jumpflag, t_bool go, t_bool record, t_bool recordprev,
    control_state statecontrol, char playfadeflag, char recfadeflag,
    long playfade, long minloop, long maxloop, long initiallow,
    long initialhigh, t_bool loopdetermine, long startloop, long endloop,
    double overdubamp, char recendmark, t_bool append)
{
    if (dirt) {
        buffer_setdirty(buf);
    }
    buffer_unlocksamples(buf);

    if (x->clockgo) {
        clock_delay(x->tclock, 0);
        x->clockgo = 0;
    } else if ((!go) || (x->reportlist <= 0)) {
        clock_unset(x->tclock);
        x->clockgo = 1;
    }

    // Update object state
    x->o1prev = o1prev;
    x->o1dif = o1dif;
    x->writeval1 = writeval1;
    x->maxhead = maxhead;
    x->pokesteps = pokesteps;
    x->wrapflag = wrapflag;
    x->snrfade = snrfade;
    x->playhead = accuratehead;
    x->directionorig = directionorig;
    x->directionprev = directionprev;
    x->recordhead = recordhead;
    x->alternateflag = alternateflag;
    x->recordfade = recordfade;
    x->triginit = triginit;
    x->jumpflag = jumpflag;
    x->go = go;
    x->record = record;
    x->recordprev = recordprev;
    x->statecontrol = statecontrol;
    x->playfadeflag = playfadeflag;
    x->recfadeflag = recfadeflag;
    x->playfade = playfade;
    x->minloop = minloop;
    x->maxloop = maxloop;
    x->initiallow = initiallow;
    x->initialhigh = initialhigh;
    x->loopdetermine = loopdetermine;
    x->startloop = startloop;
    x->endloop = endloop;
    x->overdubprev = overdubamp;
    x->recendmark = recendmark;
    x->append = append;
}

void karma_mono_perform(
    t_karma* x, t_object* dsp64, double** ins, long nins, double** outs,
    long nouts, long vcount, long flgs, void* usr)
{
    // Input/output setup
    long    syncoutlet = x->syncoutlet;
    double* in1 = ins[0];                     // mono in
    double* in2 = ins[1];                     // speed (if signal connected)
    double* out1 = outs[0];                   // mono out
    double* outPh = syncoutlet ? outs[1] : 0; // sync (if @syncout 1)
    long    n = vcount;
    short   speedinlet = x->speedconnect;

    // State variables
    control_state   statecontrol;
    interp_type     interp;
    switchramp_type snrtype;

    double accuratehead, maxhead, jumphead, srscale, speedsrscaled, recplaydif,
        pokesteps;
    double speed, speedfloat, osamp1, overdubamp, overdubprev, ovdbdif,
        selstart, selection;
    double o1prev, o1dif, frac, snrfade, globalramp, snrramp, writeval1, coeff1,
        recin1;
    t_bool go, record, recordprev, alternateflag, loopdetermine, jumpflag,
        append, dirt, wrapflag, triginit;
    char direction, directionprev, directionorig, playfadeflag, recfadeflag,
        recendmark;
    long playfade, recordfade, i, interp0, interp1, interp2, interp3, pchans;
    long frames, startloop, endloop, playhead, recordhead, minloop, maxloop,
        setloopsize;
    long initiallow, initialhigh;

    // Buffer setup
    t_buffer_obj* buf = buffer_ref_getobject(x->buf);
    float*        b = buffer_locksamples(buf);

    // Early exit if no buffer or disabled
    record = x->record;
    recordprev = x->recordprev;
    dirt = 0;
    if (!b || x->k_ob.z_disabled)
        goto zero;
    if (record || recordprev)
        dirt = 1;
    if (x->buf_modified) {
        karma_buf_modify(x, buf);
        x->buf_modified = false;
    }

    // Load state from object
    o1prev = x->o1prev;
    o1dif = x->o1dif;
    writeval1 = x->writeval1;
    go = x->go;
    statecontrol = x->statecontrol;
    playfadeflag = x->playfadeflag;
    recfadeflag = x->recfadeflag;
    recordhead = x->recordhead;
    alternateflag = x->alternateflag;
    pchans = x->bchans;
    srscale = x->srscale;
    frames = x->bframes;
    triginit = x->triginit;
    jumpflag = x->jumpflag;
    append = x->append;
    directionorig = x->directionorig;
    directionprev = x->directionprev;
    minloop = x->minloop;
    maxloop = x->maxloop;
    initiallow = x->initiallow;
    initialhigh = x->initialhigh;
    selection = x->selection;
    loopdetermine = x->loopdetermine;
    startloop = x->startloop;
    selstart = x->selstart;
    endloop = x->endloop;
    recendmark = x->recendmark;
    overdubamp = x->overdubprev;
    overdubprev = x->overdubamp;
    ovdbdif = (overdubamp != overdubprev) ? ((overdubprev - overdubamp) / n)
                                          : 0.0;
    recordfade = x->recordfade;
    playfade = x->playfade;
    accuratehead = x->playhead;
    playhead = trunc(accuratehead);
    maxhead = x->maxhead;
    wrapflag = x->wrapflag;
    jumphead = x->jumphead;
    pokesteps = x->pokesteps;
    snrfade = x->snrfade;
    globalramp = (double)x->globalramp;
    snrramp = (double)x->snrramp;
    snrtype = x->snrtype;
    interp = x->interpflag;
    speedfloat = x->speedfloat;

    // Process state control
    karma_perform_state_control(
        x, &statecontrol, &record, &go, &triginit, &loopdetermine,
        &alternateflag, &recendmark, &recordfade, &playfade, &recfadeflag,
        &playfadeflag, &snrfade);

    // Main processing loop
    while (n--) {
        // Process input
        recin1 = *in1++;
        speed = speedinlet ? *in2++ : speedfloat;
        direction = (speed > 0) ? 1 : ((speed < 0) ? -1 : 0);

        // Handle direction changes
        karma_perform_direction_change(
            x, direction, directionprev, record, globalramp, b, frames, pchans,
            recordhead, &snrfade, &recordfade, &recfadeflag, &recordhead);

        // Handle record state changes
        karma_perform_record_state_change(
            x, record, recordprev, globalramp, b, frames, pchans, recordhead,
            accuratehead, direction, speed, &snrfade, &recordfade, &recfadeflag,
            &recordhead, &dirt);

        // Process loop playback or initial loop creation
        if (!loopdetermine) {
            // Normal loop playback - simplified for now
            if (go) {
                // Basic playback logic - this is a simplified version
                setloopsize = maxloop - minloop;
                speedsrscaled = speed * srscale;
                accuratehead = accuratehead + speedsrscaled;

                // Basic boundary checking
                if (directionorig >= 0) {
                    if (accuratehead > maxloop) {
                        accuratehead = accuratehead - setloopsize;
                        snrfade = 0.0;
                    } else if (accuratehead < 0.0) {
                        accuratehead = maxloop + accuratehead;
                        snrfade = 0.0;
                    }
                } else {
                    if (accuratehead > (frames - 1)) {
                        accuratehead = ((frames - 1) - setloopsize)
                            + (accuratehead - (frames - 1));
                        snrfade = 0.0;
                    } else if (accuratehead < ((frames - 1) - maxloop)) {
                        accuratehead = (frames - 1)
                            - (((frames - 1) - setloopsize) - accuratehead);
                        snrfade = 0.0;
                    }
                }
            } else {
                osamp1 = 0.0;
            }
        } else {
            // Initial loop creation - simplified for now
            if (go) {
                setloopsize = maxloop - minloop;
                speedsrscaled = speed * srscale;
                accuratehead = accuratehead + speedsrscaled;

                // Track maximum distance traversed
                if (direction == directionorig) {
                    if (accuratehead > (frames - 1)) {
                        accuratehead = 0.0;
                        record = append;
                        recendmark = triginit = 1;
                        loopdetermine = alternateflag = 0;
                        maxhead = frames - 1;
                    } else if (accuratehead < 0.0) {
                        accuratehead = frames - 1;
                        record = append;
                        recendmark = triginit = 1;
                        loopdetermine = alternateflag = 0;
                        maxhead = 0.0;
                    } else {
                        if (((directionorig >= 0) && (maxhead < accuratehead))
                            || ((directionorig < 0)
                                && (maxhead > accuratehead))) {
                            maxhead = accuratehead;
                        }
                    }
                }
            }

            osamp1 = 0.0;
        }

        // Process interpolation and output
        if (!loopdetermine && go) {
            osamp1 = karma_perform_interpolation(
                x, accuratehead, direction, directionorig, maxloop, frames, b,
                pchans, interp, &playhead, &interp0, &interp1, &interp2,
                &interp3);

            // Process playback effects
            karma_perform_playback_processing(
                x, osamp1, globalramp, snrramp, snrtype, &snrfade, &playfade,
                playfadeflag, go, record, triginit, jumpflag, loopdetermine,
                &o1dif, o1prev, &osamp1, &go, &triginit, &jumpflag,
                &loopdetermine, &snrfade, &playfade);
        }

        // Output audio
        o1prev = osamp1;
        *out1++ = osamp1;
        if (syncoutlet) {
            setloopsize = maxloop - minloop;
            *outPh++ = (directionorig >= 0)
                ? ((accuratehead - minloop) / setloopsize)
                : ((accuratehead - (frames - setloopsize)) / setloopsize);
        }

        // Process recording
        karma_perform_recording(
            x, recin1, record, globalramp, b, frames, pchans, playhead,
            overdubamp, recfadeflag, recordfade, recordhead, writeval1,
            pokesteps, &recin1, &recordhead, &writeval1, &pokesteps, &dirt);

        // Process record fade
        karma_perform_record_fade_processing(
            x, globalramp, &recordfade, recfadeflag, record, triginit, jumpflag,
            recendmark, loopdetermine, directionorig, frames, maxhead, &record,
            &triginit, &jumpflag, &loopdetermine, &recendmark, &maxloop);

        directionprev = direction;

        // Update overdub amplitude
        if (ovdbdif != 0.0)
            overdubamp = overdubamp + ovdbdif;

        initialhigh = (dirt) ? maxloop : initialhigh;
    }

    // Cleanup and state update
    karma_perform_cleanup(
        x, buf, dirt, go, o1prev, o1dif, writeval1, maxhead, pokesteps,
        wrapflag, snrfade, accuratehead, directionorig, directionprev,
        recordhead, alternateflag, recordfade, triginit, jumpflag, go, record,
        recordprev, statecontrol, playfadeflag, recfadeflag, playfade, minloop,
        maxloop, initiallow, initialhigh, loopdetermine, startloop, endloop,
        overdubamp, recendmark, append);

    return;

zero:
    while (n--) {
        *out1++ = 0.0;
        if (syncoutlet)
            *outPh++ = 0.0;
    }

    return;
}
