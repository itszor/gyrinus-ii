/* Gyrinus II - Son of Gyrinus (Tom Cooper, 1991; public domain): what
 * libroger needs to know about the game, and the BASIC parts of its start-up
 * translated by hand.
 *
 * !Run (the 2MB+ version) runs, in order: Code.Warnung (BASIC: a warning
 * screen for 5s or until a key or click), Code.Loading (BASIC: the loading
 * screen), *RMRun MemAlloc and its memory-size commands, Soundfx.Loadfx2Meg
 * (*RMLoad of 16 sample voice modules), Code.RunMe (BASIC: the title
 * picture, the music variables, then *Run Code.MCode - the game) and, when
 * the game returns, Code.EndGame (BASIC: the credits screen until Space).
 * The BASIC programs are written out in ../basic (tools/detok.py); each one
 * assembles the same RLE decoder with BASIC's inline assembler, unpacks a
 * sprite file with it and plots sprites into both screen banks. Here they
 * are C (warnung, loading, runme, endgame below). MemAlloc only resizes
 * memory areas, which libroger fixes (screen_size); the voice modules are
 * translated (gen/v_*) and RMLoaded at start-up as by Loadfx2Meg, whose
 * *Tuning commands are loadfx() below. */
#include <stdio.h>
#include <string.h>
#include <roger.h>

extern const struct roger_image recomp_image;      /* gen/mcode (gen_c.py) */
#include "gen/modules.inc"                         /* the voices and the music player */

#define AREA     0x10000u      /* the BASIC programs' DIM'd buffers: the packed sprite file */
#define DECRUNCH 0x40000u      /* ...and the unpacked sprite area (both below MCode at &7F800) */

/* ---------------------------------------------------------------- BASIC
 * SYS and VDU as the BASIC programs use them */
static void sys(uint32_t n, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5)
{
    R[0] = r0; R[1] = r1; R[2] = r2; R[3] = r3; R[4] = r4; R[5] = r5;
    roger_swi(n);
}
static void vdu(const uint8_t *s, int n) { for (int k = 0; k < n; k++) sys(0, s[k], 0, 0, 0, 0, 0); }

/* the decoder each program assembles (PROCassembl): a byte-wise RLE. The
 * source starts with its length; a byte equal to the previous one is
 * followed by a count of further copies, and the run doesn't continue
 * into the next byte. */
static void decode(uint32_t src, uint32_t dst)
{
    int32_t len = (int32_t)RD32(src);
    uint32_t last = 0xFFFFFFFF;
    src += 4;
    do {
        uint32_t byte = MEM[src++];
        if (byte == last) {
            int rep = MEM[src++] + 1;
            len--;
            while (--rep >= 0) MEM[dst++] = byte;
            last = 0xFFFFFFFF;
        } else {
            MEM[dst++] = byte;
            last = byte;
        }
    } while (--len > 0);
}

struct plot { const char *sprite; int x, y; };

/* PROCloadingscreen & co: load and unpack a sprite file, MODE 13 without a
 * cursor, and plot sprites into both banks (OS_SpriteOp 34+256: by name,
 * user area, at OS coordinates) */
static void screen(const char *file, const struct plot *p)
{
    static const uint8_t mode13[] = {22, 13, 23, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    char cmd[256];
    snprintf(cmd, sizeof cmd, "Load %s %X", file, AREA);
    roger_cli(cmd);
    decode(AREA, DECRUNCH);
    vdu(mode13, sizeof mode13);
    for (int bank = 1; bank <= 2; bank++) {
        sys(6, 112, bank, 0, 0, 0, 0);               /* *FX 112: VDU drivers' bank */
        vdu((const uint8_t[]){12}, 1);               /* CLS */
        for (const struct plot *q = p; q->sprite; q++)
            sys(0x2E, 290, DECRUNCH, roger_scratch(q->sprite), q->x, q->y, 0);
    }
}

static uint32_t now(void) { sys(0x42, 0, 0, 0, 0, 0, 0); return R[0]; }   /* OS_ReadMonotonicTime */

/* Code.Warnung: until a key, a click or 5s (REPEAT MOUSE x%,y%,b% UNTIL
 * INKEY$(0)<>"" OR b% OR TIME>t%+500) */
static void warnung(void)
{
    screen("<Gyr2$Dir>.Grafix.Warnung", (const struct plot[]){{"warnung", 0, 0}, {NULL, 0, 0}});
    uint32_t t = now();
    for (;;) {
        sys(0x1C, 0, 0, 0, 0, 0, 0);                 /* OS_Mouse */
        if (R[2]) break;
        sys(6, 129, 1, 0, 0, 0, 0);                  /* INKEY, waiting 1cs (keeps the display going) */
        if (R[2] == 0) break;
        if (now() - t > 500) break;
    }
}

/* Code.Loading */
static void loading(void)
{
    screen("<Gyr2$Dir>.Grafix.LoadingC",
           (const struct plot[]){{"gyrinusload", 110, 372}, {"gyrinusload2", 474, 352}, {NULL, 0, 0}});
}

/* Code.RunMe, up to *Run Code.MCode. PROCsetsystemvars: the tunes are on
 * and the title tune continues into the game unless the RMA is short
 * (*RMAFree 108k / 400k fail and run their *Set) - never here. VDU 21
 * leaves the VDU drivers off until MCode's VDU 6. */
static void runme(void)
{
    screen("<Gyr2$Dir>.Grafix.TitleCLoad", (const struct plot[]){{"TPLoad", 0, 0}, {NULL, 0, 0}});
    roger_cli("Set Gyr$TuneOn TuneOn");
    roger_cli("Set Gyr$TuneCont Continue");
    vdu((const uint8_t[]){21}, 1);
}

/* Soundfx.Loadfx2Meg, after its *RMLoads (the voices are loaded as
 * configured modules): *Tuning is relative, so this sets the tuning three
 * octaves below the default, bringing the effects' samples (played at
 * BBC pitch 50 or so) down to roughly the rates they were recorded at */
static void loadfx(void)
{
    roger_cli("Tuning 0");
    roger_cli("Tuning -&1000");
    roger_cli("Tuning -&2000");
}

/* !Run's start-up, before MCode runs */
static void init(void)
{
    if (roger_interactive()) { warnung(); loading(); }
    loadfx();
    runme();
}

/* Code.EndGame, after MCode returns: the credits until Space, then MODE 12
 * and "Bye Bye." */
static void endgame(void)
{
    if (!roger_interactive()) return;
    vdu((const uint8_t[]){6}, 1);
    screen("<Gyr2$Dir>.Grafix.EndScreenC", (const struct plot[]){
        {"CREDIT", 112, 400}, {"HELLO2", 68, 0}, {"HELLO", 68, 244}, {NULL, 0, 0}});
    do sys(6, 19, 0, 0, 0, 0, 0);                    /* REPEAT UNTIL INKEY(-99) */
    while (sys(6, 129, 256 - 99, 0xFF, 0, 0, 0), R[1] != 0xFF);
    static const uint8_t mode12[] = {22, 12, 23, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    vdu(mode12, sizeof mode12);
    static const char bye[] = "Bye Bye.\r\n";
    vdu((const uint8_t *)bye, sizeof bye - 1);
}

/* The CoconizerSong player claims EventV for the vsync event (4) and
 * advances the tune on every event it gets without looking at the event
 * number: in the original, keypress events (11) made the music run faster
 * while keys were pressed. libroger only passes on enabled events (the
 * player enables just 4), but to keep the tune at vsync rate whatever else
 * gets enabled, other events go straight past the handler (a pass-on to
 * R14, as a claimant that doesn't want the event does). */
#define COCO_BASE   0x1840000u                    /* ../tools/images.py */
#define COCO_EVENTV (COCO_BASE + 0x664)

static int dispatch(uint32_t a)
{
    if (a == COCO_EVENTV && R[0] != 4) { npc = R[14]; return 1; }
    return 0;
}

/* The headless scenario (--limit runs; instruction counts): Space on the
 * title page starts a game, Space again answers "insert disk two", then X
 * walks right in the first zone */
static const struct roger_key keys[] = {
    {99, 100000000, 110000000},                    /* Space */
    {99, 150000000, 160000000},                    /* Space */
    {67, 250000000, 300000000},                    /* X */
};

static const char *const variables[] = {
    "Gyr2$Dir", "!Gyrinus2",                          /* !Run: Set Gyr2$Dir <Obey$Dir> */
    "Gyr2Mapz$Dir", "!Gyrn2Mapz",                     /* the maps disc's application */
    NULL};

static const struct roger_config config = {
    .title = "Gyrinus II (recompiled)",
    /* MCode: load address &7F800, execution address &8BD08 (from the
     * disc's catalogue; the extracted name doesn't keep the latter) */
    .image_path = "../extracted/!Gyrinus2/Code/MCode,0007f800-00000000",
    .entry = 0x8BD08,
    .image = &recomp_image,
    .modules = modules,
    .appdir = "../extracted",
    .variables = variables,
    .initial_mode = 13,
    /* WimpSlot 524K, which a 2MB machine's 32K pages make 544K: MCode ends
     * at &8C4C4 and puts its stack at &8D1C4 */
    .wimpslot = 544 * 1024,
    .screen_size = 160 * 1024,                     /* *ScreenSize 160k */
    .keys = keys,
    .nkeys = sizeof keys / sizeof keys[0],
    .init = init,
    .exited = endgame,
    .call_addr = dispatch,
};

int main(int argc, char **argv) { roger_main(argc, argv, &config); }
