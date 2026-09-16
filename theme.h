/* Classic defaults. Use Visualizer Settings for live customization. */
#define BAR_COUNT 42 /* Audio analysis bands, independent of screen width. */
#define BAR_SPACING 30.0 /* Logical pixels per visible bar, including gap. */
#define MAX_VISIBLE_BARS 512
#define BAR_HEIGHT_RATIO 0.48
#define BAR_WIDTH_RATIO 0.72
#define FPS 30
#define OPACITY 0.60
static const unsigned int palette[] = {
    0xffdf7e, 0xe4d783, 0xaaba87, 0x719f99, 0x267e96,
    0x086b94, 0x385e8a, 0x65517f, 0x9e3969, 0xcc285b,
    0xec6065, 0xf09b75, 0xffcf7b, 0xffdf7e, 0xe4d783,
    0xaaba87, 0x719f99, 0x267e96
};

#define ATTACK_SECONDS 0.055f
#define RELEASE_SECONDS 0.24f
#define BASS_GAIN_DB -4.0f
#define TREBLE_GAIN_DB 8.0f
#define BASE_ALPHA 0.22
