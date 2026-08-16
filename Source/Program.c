#include "Libraries/raylib.h"
#include <math.h>
#include <stddef.h>

typedef unsigned char        u8;
typedef unsigned short       u16;
typedef unsigned int         u32;
typedef unsigned long long   u64;

typedef signed char          i8;
typedef signed short         i16;
typedef signed int           i32;
typedef signed long long     i64;

typedef float                f32;
typedef double               f64;

const i32 DesignWidth             = 1920;
const i32 DesignHeight            = 1080;
const i32 MinWindowWidth          = 854;
const i32 MinWindowHeight         = 480;
const f32 WindowScreenMargin      = 0.9f;
const i32 WindowTopMargin         = 60;

const i32 DesignLeftOffset        = 950;
const i32 DesignCursorGutter      = 130;
const f32 DesignImageScale        = 1.74f;
const i32 DesignTitleFontSize     = 65;
const i32 DesignBodyFontSize      = 20;
const i32 DesignSmallFontSize     = 18;
const i32 DesignMenuFontSize      = 20; // 20 is about the practical ceiling, the three columns reach 1659px of the 1920 wide screen
const i32 DesignMenuTitleFontSize = 30;

const i32 ImageCanvasScanWidth    = 600;
const i32 ImageCanvasScanHeight   = 550;

const i32 ImageScanWidth          = 600;
const i32 ImageScanHeight         = 430;

const f32 RevealCharsPerSecond    = 15.0f;

const f32 MenuExitHoldSeconds     = 1.0f;

const f32 IntroRevealCharsPerSecond = 16.0f;
const f32 IntroHoldSeconds          = 2.0f;
const f32 IntroFadeSeconds          = 1.0f;
const f32 IntroHintDelaySeconds     = 1.5f;
const f32 IntroHintFadeSeconds      = 0.8f;
const f32 IntroQuoteWidthFraction   = 0.8f;

const i32 DesignQuoteLineGap        = 18;
const i32 DesignQuoteAttributionGap = 46;
const i32 DesignQuoteHintMargin     = 90;

const f32 FontSpacing             = 0.0f;

f32 UIScale = 0.0f;
f32 MusicCursor = 0.0f;

i32 LeftOffset;
f32 ImageScale;

i32 TitleFontSize;
i32 BodyFontSize;
i32 SmallFontSize;
i32 MenuFontSize;
i32 MenuTitleFontSize;

f32  MenuEscapeHeld   = 0.0f;

bool bPaused          = false;
bool bMenuOpen        = false;
bool bMenuEscapeArmed = false;
bool bQuitRequested   = false;

// the record does not start turning until the opening quote is done or skipped
bool bIntroActive     = true;
f32  IntroElapsed     = 0.0f;

// how many scan lines of raw audio trail behind the cursor in the waveform strip
u32 NumScanlinesToDraw = 6;

Music GoldenMusic    = {0};
Wave  GoldenWav      = {0};
f32*  GoldenSamples  = NULL;

i32 Scaled(i32 DesignPixels)
{
    i32 Result = (i32)(DesignPixels * UIScale + 0.5f);

    if (DesignPixels > 0 && Result < 1)
    {
        Result = 1;
    }

    return Result;
}

typedef struct
{
    Font Title;
    Font Body;
    Font Small;
    Font Menu;
} UIFonts;

typedef struct
{
    i32 BaseLocationX;
    i32 BaseLocationY;
    i32 ColumnWidth;
    i32 LeftColumnX;
    i32 RightColumnX;
    i32 CursorY;
    i32 NameY;
    i32 SourceY;
    i32 DescriptionY;
} UILayout;

typedef u8 EImageColorChannel;
enum
{
    Mono  = 0,
    Red   = 1,
    Green = 2,
    Blue  = 3
};

typedef u8 EEdgeTrackingState;
enum
{
    Idle     = 0,
    Tracking = 1
};

typedef struct
{
    f32 FallThreshold;
    u32 DebounceSamples;
} ScanTriggerThresholdParams;

typedef struct
{
    const char* Place;
    f32 Latitude;
    f32 Longitude;
} GeoLocation;

typedef struct
{
    const char* Name;
    const char* Source;
    const char* Description;
    const char* SourceURL;
    GeoLocation Location;
    ScanTriggerThresholdParams* OverrideParams;
    u32 SampleOffset;
    EImageColorChannel ColorChannel;
    bool bPortrait;
    bool bPortraitAntiClockwise;
} ImageMetaData;

typedef struct
{
    KeyboardKey Key;
    KeyboardKey ShiftKey;
    ImageMetaData LeftImage;
    ImageMetaData RightImage;
} ImageMapping;

typedef struct
{
    i32 MappingIndex;
    f32 Elapsed;
} TextReveal;

typedef struct
{
    u32 ImageOffset;
    f32 Threshold;
    u32 Cursor;
    u32 ScanLine;
    Image* ScanImage;
    Texture2D ScanTexture;
    TextReveal RevealState;

    ImageMetaData MetaData;
    i32 ChannelIndex;

    bool bLeftChannel;
    bool bDrawWaveform;
} RecordPlayer;

UIFonts Fonts = {0};

RecordPlayer Player_LeftChannel  = {0};
RecordPlayer Player_RightChannel = {0};

const char* IntroQuoteLines[] =
{
    "Billions of years from now our sun, then a distended",
    "red giant star, will have reduced Earth to a charred",
    "cinder.",
    "But the Voyager record will still be largely",
    "intact, in some other remote region of the Milky Way",
    "galaxy, preserving a murmur of an ancient",
    "civilization that once flourished ..."
};

const char* IntroAttribution = "Carl Sagan, Murmurs of Earth (1978)";

#define SAMPLES_FACTOR (1.0f/(60.0f))

// TODO: export to jpeg
// TODO: add descriptions to almost all images
// TODO: fix crash when the music is finished/reached the end. loop back to beginning instead

// names referenced from: 
//    https://science.nasa.gov/mission/voyager/golden-record-contents/images/
//    https://en.wikipedia.org/wiki/Contents_of_the_Voyager_Golden_Record

// no published credit exists for The Sun or Heron Island (record 41):
// both credit cells are blank on Wikipedia and neither has a NASA image-detail page.

// yes i really did type this by hand
ImageMapping ImageMappings[] =
{
    {.Key = KEY_ONE,         .LeftImage.Name = "Calibration Circle",                    .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 1375283,  .RightImage.SampleOffset = 1490087,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "David Doubilet", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/calibration-circle-31325346536-o/"},
    {.Key = KEY_TWO,         .LeftImage.Name = "Solar Location in the Milky Way",       .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 1905722,  .RightImage.SampleOffset = 1984822,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Frank Drake", .RightImage.Source = "David Doubilet", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.0275f, .DebounceSamples = 3}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-location-map-30992503150-o/"},
    {.Key = KEY_THREE,       .LeftImage.Name = "Math Definitions",                      .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 2446590,  .RightImage.SampleOffset = 2505754,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "David Doubilet", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/mathematical-definitions-30539954574-o/"},
    {.Key = KEY_FOUR,        .LeftImage.Name = "Physics Definitions",                   .RightImage.Name = "Tree Toad",                                 .LeftImage.SampleOffset = 2968481,  .RightImage.SampleOffset = 3033022,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Dave Wickstrom", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.03f, .DebounceSamples = 2}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/physical-unit-definitions-30554003853-o/"},
    {.Key = KEY_FIVE,        .LeftImage.Name = "Solar System Parameters 1",             .RightImage.Name = "Crocodile",                                 .LeftImage.SampleOffset = 3483550,  .RightImage.SampleOffset = 3546161,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Peter Beard", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-system-parameters-30554017073-o/"},
    {.Key = KEY_SIX,         .LeftImage.Name = "Solar System Parameters 2",             .RightImage.Name = "Eagle",                                     .LeftImage.SampleOffset = 3998630,  .RightImage.SampleOffset = 4051738,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Donona, Taplinger Publishing Co.", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-system-parameters-30992729550-o/"},
    {.Key = KEY_SEVEN,       .LeftImage.Name = "The Sun",                               .RightImage.Name = "Zebras",                                    .LeftImage.SampleOffset = 4500698,  .RightImage.SampleOffset = 4544028,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Description = "Hale Observations", .RightImage.Source = "South African Tourist Corp."},
    {.Key = KEY_EIGHT,       .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 4984064,  .RightImage.SampleOffset = 5036615,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Red,   .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall", .RightImage.bPortrait = true, .RightImage.Location = {"Gombe Stream, Tanzania", -4.6700f, 29.6300f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-spectrum-30992778240-o/"},
    {.Key = KEY_NINE,        .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 5467791,  .RightImage.SampleOffset = 5543004,  .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Green, .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall", .RightImage.bPortrait = true, .RightImage.Location = {"Gombe Stream, Tanzania", -4.6700f, 29.6300f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-spectrum-30992778240-o/"},
    {.Key = KEY_NULL,        .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 5955425,  .RightImage.SampleOffset = 6039902,  .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall", .RightImage.bPortrait = true, .RightImage.Location = {"Gombe Stream, Tanzania", -4.6700f, 29.6300f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/solar-spectrum-30992778240-o/"},
    {.Key = KEY_ZERO,        .LeftImage.Name = "Mercury",                               .RightImage.Name = "Sketch of Bushmen Hunters",                 .LeftImage.SampleOffset = 6582824,  .RightImage.SampleOffset = 6570356,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "Jon Lomberg", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/mercury-31246953391-o/", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/sketch-of-bushmen-30541167404-o/"},
    {.Key = KEY_Q,           .LeftImage.Name = "Mars",                                  .RightImage.Name = "Bushmen Hunters",                           .LeftImage.SampleOffset = 7091405,  .RightImage.SampleOffset = 7067636,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "R. Farbman; Time Inc.", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/mars-30992814480-o/"},
    {.Key = KEY_W,           .LeftImage.Name = "Jupiter",                               .RightImage.Name = "Man from Guatemala",                        .LeftImage.SampleOffset = 7615711,  .RightImage.SampleOffset = 7573015,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.bPortrait = true, .LeftImage.bPortraitAntiClockwise = true,.RightImage.Source = "U.N. Photo", .RightImage.bPortrait = true, .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.043f, .DebounceSamples = 2}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/jupiter-31325748356-o/", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/man-from-guatemala-30541182514-o/"},
    {.Key = KEY_E,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Dancer from Bali",                          .LeftImage.SampleOffset = 8137606,  .RightImage.SampleOffset = 8086172,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.bPortrait = true, .LeftImage.bPortraitAntiClockwise = true,.RightImage.Source = "Donna Grosvenor", .RightImage.bPortrait = true, .RightImage.Location = {"", -8.4095f, 115.1889f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/earth-31326146966-o/"},
    {.Key = KEY_R,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Andean Girls",                              .LeftImage.SampleOffset = 8635865,  .RightImage.SampleOffset = 8592929,  .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.bPortrait = true, .LeftImage.bPortraitAntiClockwise = true,.RightImage.Source = "Joseph Scherschel", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/earth-31326146966-o/"},
    {.Key = KEY_T,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Thailand Master Craftsman",                 .LeftImage.SampleOffset = 9142947,  .RightImage.SampleOffset = 9089631,  .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.bPortrait = true, .LeftImage.bPortraitAntiClockwise = true,.RightImage.Source = "Dean Conger", .RightImage.bPortrait = true, .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.05f, .DebounceSamples = 2}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/earth-31326146966-o/"},
    {.Key = KEY_Y,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Elephant",                                  .LeftImage.SampleOffset = 9636814,  .RightImage.SampleOffset = 9600311,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.Source = "Peter Kunstadter", .LeftImage.Location = {"", 29.5000f, 33.8000f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/egypt-red-sea-sinal-peninsula-and-the-nile-30993198280-o/"},
    {.Key = KEY_U,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Old Man Smoking",                           .LeftImage.SampleOffset = 10113065, .RightImage.SampleOffset = 10102506, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.bPortrait = true, .RightImage.Source = "Jonathon Blair", .RightImage.Description = "Turkey", .LeftImage.Location = {"", 29.5000f, 33.8000f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/egypt-red-sea-sinal-peninsula-and-the-nile-30993198280-o/"},
    {.Key = KEY_I,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Old Man with Dog & Flowers",                .LeftImage.SampleOffset = 10615931, .RightImage.SampleOffset = 10613180, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.bPortrait = true, .RightImage.Source = "Bruce Baumann", .LeftImage.Location = {"", 29.5000f, 33.8000f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/egypt-red-sea-sinal-peninsula-and-the-nile-30993198280-o/"},
    {.Key = KEY_O,           .LeftImage.Name = "Chemical Definitions",                  .RightImage.Name = "Mountain Climber",                          .LeftImage.SampleOffset = 11130449, .RightImage.SampleOffset = 11125216, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Gaston Rebuffat", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/chemical-definitions-31218371762-o/"},
    {.Key = KEY_P,           .LeftImage.Name = "DNA Structure",                         .RightImage.Name = "Gymnast",                                   .LeftImage.SampleOffset = 11611579, .RightImage.SampleOffset = 11631241, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Philip Leonian; Sports Illustrated", .RightImage.Description = "Cathy Rigby", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/dna-structure-31362211895-o/"},
    {.Key = KEY_A,           .LeftImage.Name = "DNA Structure (Magnified)",             .RightImage.Name = "Sprinters",                                 .LeftImage.SampleOffset = 12112408, .RightImage.SampleOffset = 12131300, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "History of the Olympics, Picturepoint, London", .RightImage.Description = "Valeriy Borzov of the U.S.S.R. in lead", .RightImage.Location = {"Olympiastadion, Munich", 48.1731f, 11.5468f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/dna-structure-magnified-light-hit-31362224015-o/", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/sprinters-valeri-borzov-of-the-ussr-in-lead-history-of-the-olympics-31363259165-o/"},
    {.Key = KEY_S,           .LeftImage.Name = "Cell Division",                         .RightImage.Name = "Schoolroom",                                .LeftImage.SampleOffset = 12618348, .RightImage.SampleOffset = 12643425, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Turtox/Cambosco", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Japan", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/schoolroom-30994523250-o/"},
    {.Key = KEY_D,           .LeftImage.Name = "Anatomy 1",                             .RightImage.Name = "Children with Globe",                       .LeftImage.SampleOffset = 13155793, .RightImage.SampleOffset = 13158591, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Skeleton & Muscles, Front", .LeftImage.bPortrait = true, .RightImage.Source = "U.N. Photo", .RightImage.Description = "U.N. International School", .RightImage.Location = {"", 40.7370f, -73.9720f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/children-with-globe-30541853554-o/"},
    {.Key = KEY_F,           .LeftImage.Name = "Anatomy 2",                             .RightImage.Name = "Cotton Harvest",                            .LeftImage.SampleOffset = 13667626, .RightImage.SampleOffset = 13664412, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Skeleton & Muscles, Back",  .LeftImage.bPortrait = true, .RightImage.Source = "Howell Walker"},
    {.Key = KEY_G,           .LeftImage.Name = "Anatomy 3",                             .RightImage.Name = "Grape Picker",                              .LeftImage.SampleOffset = 14169422, .RightImage.SampleOffset = 14179145, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Lungs & Kidneys, Back",     .LeftImage.bPortrait = true, .RightImage.Source = "David Moore"},
    {.Key = KEY_H,           .LeftImage.Name = "Anatomy 4",                             .RightImage.Name = "Supermarket",                               .LeftImage.SampleOffset = 14671718, .RightImage.SampleOffset = 14694679, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Lungs & Kidneys, Front",    .LeftImage.bPortrait = true, .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Woman eating grapes", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/supermarket-30555896943-o/"},
    {.Key = KEY_J,           .LeftImage.Name = "Anatomy 5",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 15186181, .RightImage.SampleOffset = 15192711, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Back",     .LeftImage.bPortrait = true, .RightImage.Source = "Jerry Greenberg", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.027f, .DebounceSamples = 3}},
    {.Key = KEY_K,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 15679226, .RightImage.SampleOffset = 15692985, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Green, .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front",    .LeftImage.bPortrait = true, .RightImage.Source = "Jerry Greenberg"},
    {.Key = KEY_L,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 16190230, .RightImage.SampleOffset = 16224544, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Blue,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front",    .LeftImage.bPortrait = true, .RightImage.Source = "Jerry Greenberg"},
    {.Key = KEY_Z,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Fishing Boat with Nets",                    .LeftImage.SampleOffset = 16705655, .RightImage.SampleOffset = 16725175, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front",    .LeftImage.bPortrait = true, .RightImage.Source = "U.N. Photo", .RightImage.Description = "Greece", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/fishing-boat-with-nets-30542208064-o/"},
    {.Key = KEY_X,           .LeftImage.Name = "Anatomy 7",                             .RightImage.Name = "Cooking Fish",                              .LeftImage.SampleOffset = 17189421, .RightImage.SampleOffset = 17242071, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Ribcage",                   .LeftImage.bPortrait = true, .RightImage.Source = "Cooking of Spain and Portugal, Time-Life Books"},
    {.Key = KEY_C,           .LeftImage.Name = "Anatomy 8",                             .RightImage.Name = "Chinese Dinner Party",                      .LeftImage.SampleOffset = 17738425, .RightImage.SampleOffset = 17748573, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Muscles",                   .LeftImage.bPortrait = true, .RightImage.Source = "Time-Life Books", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.055f, .DebounceSamples = 4}},
    {.Key = KEY_V,           .LeftImage.Name = "Human Sex Organs",                      .RightImage.Name = "Licking, Eating and Drinking",              .LeftImage.SampleOffset = 18257464, .RightImage.SampleOffset = 18250532, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Sinauer Associates, Inc.", .LeftImage.Description = "Male & Female", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.032f, .DebounceSamples = 3}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/demonstration-of-licking-eating-and-drinking-30542224004-o/"},
    {.Key = KEY_B,           .LeftImage.Name = "Conception (Diagram)",                  .RightImage.Name = "Great Wall of China",                       .LeftImage.SampleOffset = 18765554, .RightImage.SampleOffset = 18747087, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "H. Edward Kim", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/diagram-of-conception-31218656712-o/"},
    {.Key = KEY_N,           .LeftImage.Name = "Conception",                            .RightImage.Name = "House Construction",                        .LeftImage.SampleOffset = 19277607, .RightImage.SampleOffset = 19244296, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Albert Bonniers; Forlag, Stockholm", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Cameroon", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/house-construction-african-30542255034-o/"},
    {.Key = KEY_M,           .LeftImage.Name = "Fertilized Ovum",                       .RightImage.Name = "Construction Scene",                        .LeftImage.SampleOffset = 19788079, .RightImage.SampleOffset = 19745141, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Albert Bonniers; Forlag, Stockholm", .RightImage.Source = "William Albert Allard", .RightImage.Description = "Amish Country", .RightImage.bPortrait = true},
    {.Key = KEY_COMMA,       .LeftImage.Name = "Fetus (Diagram)",                       .RightImage.Name = "House",                                     .LeftImage.SampleOffset = 20291384, .RightImage.SampleOffset = 20251104, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Ethiopia", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.032f, .DebounceSamples = 3}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/fetus-diagram-30540929914-o/", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/house-africa-30994933500-o/"},
    {.Key = KEY_PERIOD,      .LeftImage.Name = "Fetus",                                 .RightImage.Name = "House",                                     .LeftImage.SampleOffset = 20837565, .RightImage.SampleOffset = 20753634, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Dr. Frank Allan", .LeftImage.bPortrait = true, .RightImage.Source = "Robert Sisson", .RightImage.Description = "New England"},
    {.Key = KEY_SLASH,       .LeftImage.Name = "Diagram of Male and Female",            .RightImage.Name = "Modern House",                              .LeftImage.SampleOffset = 21336797, .RightImage.SampleOffset = 21271237, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg",     .LeftImage.bPortrait = true, .RightImage.Source = "Frank Drake", .RightImage.Description = "Cloudcroft, New Mexico", .RightImage.Location = {"", 32.9576f, -105.7414f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/diagram-of-male-and-female-31326553496-o/", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/modern-house-cloudcroft-new-mexico-30556337973-o/"},
    {.Key = KEY_GRAVE,       .LeftImage.Name = "Birth",                                 .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 21854824, .RightImage.SampleOffset = 21770903, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Wayne Miller",    .LeftImage.bPortrait = true, .RightImage.Source = "Jim Amos"},
    {.ShiftKey = KEY_ONE,    .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 22366812, .RightImage.SampleOffset = 22271748, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Green, .LeftImage.Source = "U.N. Photo",      .LeftImage.bPortrait = true, .LeftImage.Description = "Philippines", .RightImage.Source = "Jim Amos", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/nursing-mother-31362634275-o/"},
    {.ShiftKey = KEY_TWO,    .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 22880833, .RightImage.SampleOffset = 22784699, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Blue,  .LeftImage.Source = "U.N. Photo",      .LeftImage.bPortrait = true, .LeftImage.Description = "Philippines", .RightImage.Source = "Jim Amos", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/nursing-mother-31362634275-o/"},
    {.ShiftKey = KEY_THREE,  .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "Taj Mahal",                                 .LeftImage.SampleOffset = 23397216, .RightImage.SampleOffset = 23273941, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "U.N. Photo",      .LeftImage.bPortrait = true, .LeftImage.Description = "Philippines", .RightImage.Source = "David Carroll", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}, .RightImage.Location = {"Agra, India", 27.1751f, 78.0421f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/nursing-mother-31362634275-o/"},
    {.ShiftKey = KEY_FOUR,   .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "English City",                              .LeftImage.SampleOffset = 23914636, .RightImage.SampleOffset = 23793232, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey",    .LeftImage.bPortrait = true, .LeftImage.Description = "Malaysia", .RightImage.Source = "C.S. Lewis, Images of His World, William B. Eerdmans Publishing Co.", .RightImage.Description = "Oxford", .RightImage.Location = {"", 51.7520f, -1.2577f}},
    {.ShiftKey = KEY_FIVE,   .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "Boston",                                    .LeftImage.SampleOffset = 24433828, .RightImage.SampleOffset = 24280027, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey",    .LeftImage.bPortrait = true, .LeftImage.Description = "Malaysia", .RightImage.Source = "Ted Spiegel", .RightImage.Location = {"", 42.3601f, -71.0589f}},
    {.ShiftKey = KEY_SIX,    .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "U.N. Building (Day-time)",                  .LeftImage.SampleOffset = 24953317, .RightImage.SampleOffset = 24788532, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey",    .LeftImage.bPortrait = true, .LeftImage.Description = "Malaysia", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City", .RightImage.bPortrait = true, .RightImage.Location = {"", 40.7489f, -73.9680f}},
    {.ShiftKey = KEY_SEVEN,  .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 25442468, .RightImage.SampleOffset = 25301194, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Red,   .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City", .RightImage.bPortrait = true, .RightImage.Location = {"", 40.7489f, -73.9680f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/un-building-night-30556368033-o/"},
    {.ShiftKey = KEY_EIGHT,  .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 25948659, .RightImage.SampleOffset = 25815519, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Green, .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City", .RightImage.bPortrait = true, .RightImage.Location = {"", 40.7489f, -73.9680f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/un-building-night-30556368033-o/"},
    {.ShiftKey = KEY_NULL,   .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 26459594, .RightImage.SampleOffset = 26333242, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City", .RightImage.bPortrait = true, .RightImage.Location = {"", 40.7489f, -73.9680f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/un-building-night-30556368033-o/"},
    {.ShiftKey = KEY_NINE,   .LeftImage.Name = "Diagram of Family Ages",                .RightImage.Name = "Sydney Opera House",                        .LeftImage.SampleOffset = 26977596, .RightImage.SampleOffset = 26847380, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Mike Long", .RightImage.Location = {"", -33.8568f, 151.2153f}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/diagram-of-family-ages-31326608936-o/"},
    {.ShiftKey = KEY_ZERO,   .LeftImage.Name = "Family Portrait",                       .RightImage.Name = "Artisan with Drill",                        .LeftImage.SampleOffset = 27497919, .RightImage.SampleOffset = 27377225, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Nina Leen, Time, Inc.", .RightImage.Source = "Frank Hewlett", .RightImage.bPortrait = true},
    {.ShiftKey = KEY_Q,      .LeftImage.Name = "Diagram of Continental Drift",          .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 27990289, .RightImage.SampleOffset = 27885032, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Jon Lomberg", .LeftImage.bPortrait = true, .LeftImage.Description = "Derived from LAGEOS plaque", .RightImage.Source = "Fred Ward", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/diagram-of-continental-drift-31247808951-o/"},
    {.ShiftKey = KEY_W,      .LeftImage.Name = "Structure of the Earth",                .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 28491246, .RightImage.SampleOffset = 28404984, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Fred Ward", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.047f, .DebounceSamples = 3}, .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/structure-of-earth-31247834881-o/"},
    {.ShiftKey = KEY_E,      .LeftImage.Name = "Heron Island",                          .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 28984353, .RightImage.SampleOffset = 28930111, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "", .LeftImage.Description = "Great Barrier Reef of Australia", .RightImage.Source = "Fred Ward", .LeftImage.Location = {"", -23.4423f, 151.9148f}, .LeftImage.SourceURL = "https://science.nasa.gov/wp-content/uploads/2024/03/heron-island-great-barrier-reef-of-australia-31247924681-o.jpg"},
    {.ShiftKey = KEY_R,      .LeftImage.Name = "Seashore",                              .RightImage.Name = "Museum",                                    .LeftImage.SampleOffset = 29484247, .RightImage.SampleOffset = 29431324, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Dick Smith; Cape Neddick, Maine", .RightImage.Source = "David Cupp", .LeftImage.Location = {"", 43.1653f, -70.5912f}, .RightImage.bPortrait = true},
    {.ShiftKey = KEY_T,      .LeftImage.Name = "Snake River and Grand Tetons",          .RightImage.Name = "X-Ray of Hand",                             .LeftImage.SampleOffset = 29992082, .RightImage.SampleOffset = 29911490, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Ansel Adams", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.025f, .DebounceSamples = 3}, .LeftImage.Location = {"", 43.6644f, -110.7183f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/x-ray-of-hand-31220229722-o/"},
    {.ShiftKey = KEY_Y,      .LeftImage.Name = "Sand dunes",                            .RightImage.Name = "Woman with Microscope",                     .LeftImage.SampleOffset = 30490384, .RightImage.SampleOffset = 30419938, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "George Mobley", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Somalia", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.025f, .DebounceSamples = 3}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/woman-with-microscope-31071614760-o/"},
    {.ShiftKey = KEY_U,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Street Scene",                              .LeftImage.SampleOffset = 30991505, .RightImage.SampleOffset = 30930390, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Pakistan", .LeftImage.Location = {"", 36.9980f, -110.0985f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/street-scene-31071643420-o/"},
    {.ShiftKey = KEY_I,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Rush Hour Traffic",                         .LeftImage.SampleOffset = 31501703, .RightImage.SampleOffset = 31439778, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Thailand", .RightImage.bPortrait = true, .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}, .LeftImage.Location = {"", 36.9980f, -110.0985f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/voyager-rush-hour/"},
    {.ShiftKey = KEY_O,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Modern Highway",                            .LeftImage.SampleOffset = 31986826, .RightImage.SampleOffset = 31939644, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Ithaca, New York", .LeftImage.Location = {"", 36.9980f, -110.0985f}, .RightImage.Location = {"", 42.4440f, -76.5019f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/modern-highway-30634010913-o/"},
    {.ShiftKey = KEY_P,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Golden Gate Bridge",                        .LeftImage.SampleOffset = 32486942, .RightImage.SampleOffset = 32440627, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .LeftImage.bPortrait = true, .RightImage.Source = "Ansel Adams", .RightImage.Location = {"San Francisco, California", 37.8199f, -122.4783f}},
    {.ShiftKey = KEY_A,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Train",                                     .LeftImage.SampleOffset = 32978679, .RightImage.SampleOffset = 32945392, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .LeftImage.bPortrait = true, .RightImage.Source = "Gordon Gahan", .RightImage.Description = "United Aircraft Corporation Turbotrain"},
    {.ShiftKey = KEY_S,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Airplane in Flight",                        .LeftImage.SampleOffset = 33489509, .RightImage.SampleOffset = 33469402, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .LeftImage.bPortrait = true, .RightImage.Source = "Frank Drake", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.031f, .DebounceSamples = 3}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/airplane-in-flight-30634172743-o/"},
    {.ShiftKey = KEY_D,      .LeftImage.Name = "Leaf (Fragaria)",                       .RightImage.Name = "Airport",                                   .LeftImage.SampleOffset = 34005577, .RightImage.SampleOffset = 33984056, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Arthur Herrick", .LeftImage.bPortrait = true, .RightImage.Source = "George Hunter", .RightImage.Description = "Toronto", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.03f, .DebounceSamples = 3}, .RightImage.Location = {"", 43.6777f, -79.6248f}},
    {.ShiftKey = KEY_F,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Antarctic Expedition",                      .LeftImage.SampleOffset = 34523161, .RightImage.SampleOffset = 34490520, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .LeftImage.bPortrait = true, .RightImage.Source = "National Geographic; Great Adventures with the National Geographic", .RightImage.Description = "Commonwealth Trans-Antarctic Expedition", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_G,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Radio Telescope",                           .LeftImage.SampleOffset = 35015433, .RightImage.SampleOffset = 34999599, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .LeftImage.bPortrait = true, .RightImage.Source = "James Blair", .RightImage.Description = "Westerbork, Netherlands", .RightImage.Location = {"", 52.9145f, 6.6031f}},
    {.ShiftKey = KEY_H,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Radio Telescope",                           .LeftImage.SampleOffset = 35537491, .RightImage.SampleOffset = 35520804, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .LeftImage.bPortrait = true, .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Arecibo", .RightImage.Location = {"", 18.3442f, -66.7528f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/radio-telescope-arecibo-31442688195-o/"},
    {.ShiftKey = KEY_J,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Page of Book",                              .LeftImage.SampleOffset = 36022479, .RightImage.SampleOffset = 36038801, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .LeftImage.bPortrait = true, .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "The System of the World. Page 6 of Issac Newton's Principia Mathematica (Volume III)", .LeftImage.Location = {"", 36.4864f, -118.5658f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/page-of-book-newton-system-of-the-world-31297500982-o/"},
    {.ShiftKey = KEY_K,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 36547852, .RightImage.SampleOffset = 36582381, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Red,   .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .LeftImage.bPortrait = true, .RightImage.Source = "NASA", .RightImage.Description = "Ed White", .LeftImage.Location = {"", 36.4864f, -118.5658f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/astronaut-in-space-30620956564-o/"},
    {.ShiftKey = KEY_L,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 37076695, .RightImage.SampleOffset = 37093950, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .LeftImage.bPortrait = true, .RightImage.Source = "NASA", .RightImage.Description = "Ed White", .LeftImage.Location = {"", 36.4864f, -118.5658f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/astronaut-in-space-30620956564-o/"},
    {.ShiftKey = KEY_Z,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 37669499, .RightImage.SampleOffset = 37654503, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .LeftImage.bPortrait = true, .RightImage.Source = "NASA", .RightImage.Description = "Ed White", .LeftImage.Location = {"", 39.8043f, -75.5988f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/astronaut-in-space-30620956564-o/"},
    {.ShiftKey = KEY_X,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Titan Centaur Launch",                      .LeftImage.SampleOffset = 38155490, .RightImage.SampleOffset = 38150712, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .LeftImage.bPortrait = true, .RightImage.Source = "NASA", .LeftImage.Location = {"", 39.8043f, -75.5988f}, .RightImage.bPortrait = true, .RightImage.Location = {"Launch Complex 41, Cape Canaveral", 28.5837f, -80.5831f}, .RightImage.SourceURL = "https://science.nasa.gov/image-detail/titan-centaur-launch-31297529782-o/"},
    {.ShiftKey = KEY_C,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 38667959, .RightImage.SampleOffset = 38655546, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .LeftImage.bPortrait = true, .RightImage.Source = "David Harvey", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}, .LeftImage.Location = {"", 39.8043f, -75.5988f}},
    {.ShiftKey = KEY_V,      .LeftImage.Name = "Flying Insect with Flowers",            .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 39157401, .RightImage.SampleOffset = 39177643, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Borne on the Wind, Stephen Dalton",     .LeftImage.bPortrait = true, .LeftImage.bPortraitAntiClockwise = true, .LeftImage.Description = "Ichneumonidae", .RightImage.Source = "David Harvey"},
    {.ShiftKey = KEY_B,      .LeftImage.Name = "Diagram of Vertebrate Evolution",       .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 39651843, .RightImage.SampleOffset = 39671543, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Jon Lomberg",                           .LeftImage.bPortrait = true, .RightImage.Source = "David Harvey", .LeftImage.SourceURL = "https://science.nasa.gov/image-detail/diagram-of-vertebrate-evolution-30993842550-o/"},
    {.ShiftKey = KEY_N,      .LeftImage.Name = "Seashell",                              .RightImage.Name = "String Quartet",                            .LeftImage.SampleOffset = 40149135, .RightImage.SampleOffset = 40171212, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Harry N. Abrams, Inc.",                 .LeftImage.bPortrait = true, .LeftImage.Description = "Xancidae", .RightImage.Source = "Phillips Recordings", .RightImage.Description = "Quartetto Italiano"},
    {.ShiftKey = KEY_M,      .LeftImage.Name = "Dolphins",                              .RightImage.Name = "Violin with Music Score",                   .LeftImage.SampleOffset = 40702862, .RightImage.SampleOffset = 40670528, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Thomas Nebbia",                         .LeftImage.bPortrait = true, .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Cavatina", .RightImage.SourceURL = "https://science.nasa.gov/image-detail/violin-with-music-score-cavatina-31072637180-o/"},
};

i32 GetChannelIndexFromSampleOffset(u32 SampleOffset, bool bLeftChannel)
{
    i32 Result = -1;

    u32 NumMappings = sizeof(ImageMappings) / sizeof(ImageMappings[0]);
    for (i32 i = 0; i < NumMappings; i++)
    {
        ImageMapping M = ImageMappings[i];
        ImageMapping M2 = {0};
        bool bHaveNext = false;
        if (i+1 < NumMappings)
        {
            bHaveNext = true;
            M2 = ImageMappings[i+1];
        }

        if (bLeftChannel)
        {
            if (SampleOffset > M.LeftImage.SampleOffset && (!bHaveNext || (bHaveNext && SampleOffset < M2.LeftImage.SampleOffset)))
            {
                Result = i;
                break;
            }
        }
        else
        {
            if (SampleOffset > M.RightImage.SampleOffset && (!bHaveNext || (bHaveNext && SampleOffset < M2.RightImage.SampleOffset)))
            {
                Result = i;
                break;
            }
        }
    }

    return Result;
}

ImageMetaData GetImageMetaDataFromSampleOffset(u32 SampleOffset, bool bLeftChannel)
{
    ImageMetaData Result = {0};

    i32 ChannelIndex = GetChannelIndexFromSampleOffset(SampleOffset, bLeftChannel);
    if (ChannelIndex > -1)
    {
        if (bLeftChannel)
        {
            Result = ImageMappings[ChannelIndex].LeftImage;
        }
        else
        {
            Result = ImageMappings[ChannelIndex].RightImage;
        }
    }

    return Result;
}

f32 SyncPeak(f32* Samples, u64 TestIndex, u64 SampleWidth, u32 Channels, bool bLeft)
{
    f32 Result = 0;

    for (u64 k = 0; k < SampleWidth; k++)
    {
        f32 Value = Samples[(TestIndex+k) * Channels + (bLeft ? 0 : 1)];
        if (Value > Result) Result = Value;
    }

    return Result;
}

void DetectScanTrigger(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel, ScanTriggerThresholdParams* OverrideParams, i32* OutIndex, f32* OutValue)
{
    bool bFound = false;

    u32 SamplesCount = Wav.frameCount;

    f32 FallThreshold = 0.034f;
    u32 DebounceSamples = 3;
    f32 NoiseHisteresis = 0.001f;

    if (OverrideParams)
    {
        FallThreshold = OverrideParams->FallThreshold;
        DebounceSamples = OverrideParams->DebounceSamples;
    }

    u32 State = Idle;
    u32 StableCount = 0;
    i32 TroughSampleIndex = -1;
    f32 TroughSampleValue = 0.0;

    i32 Confirmed_TroughSampleIndex = -1;
    f32 Confirmed_TroughSampleValue = Samples[(SampleOffset * Wav.channels) + (bLeftChannel ? 0 : 1)];

    for (u32 i = 0; i < SearchLength; i++)
    {
        if (SampleOffset + i >= SamplesCount)
        {
            break;
        }

        f32 Current  = Samples[(SampleOffset+i)     * Wav.channels + (bLeftChannel ? 0 : 1)];
        f32 Next     = Samples[(SampleOffset+(i+1)) * Wav.channels + (bLeftChannel ? 0 : 1)];
        f32 Diff     = Next - Current;

        switch (State)
        {
            case Idle:
            {
                if (Diff < 0 && fabsf(Diff) >= FallThreshold)
                {
                    TroughSampleIndex = i;
                    TroughSampleValue = Current;
                    StableCount = 0;
                    State = Tracking;
                    // printf("diff: %f\nIndex: %d\nValue: %f\n", Diff, TroughSampleIndex, TroughSampleValue);
                }
            }
            break;

            case Tracking:
            {
                if (Current < TroughSampleValue && Current < Confirmed_TroughSampleValue)
                {
                    TroughSampleIndex = i;
                    TroughSampleValue = Current;
                    StableCount = 0;
                    // printf("found new Index: %d\nValue: %f\n", TroughSampleIndex, TroughSampleValue);
                }
                else if (Current > (TroughSampleValue + NoiseHisteresis))
                {
                    StableCount++;
                }

                if (StableCount >= DebounceSamples)
                {
                    Confirmed_TroughSampleIndex = TroughSampleIndex;
                    Confirmed_TroughSampleValue = TroughSampleValue;
                    bFound = true;
                    // printf("found\n");

                    State = Idle;
                }
            }
            break;
        }
    }

    if (!bFound)
    {
        if (TroughSampleIndex > -1)
        {
            Confirmed_TroughSampleIndex = TroughSampleIndex;
            Confirmed_TroughSampleValue = TroughSampleValue;
            bFound = true;
        }
    }

    if (OutIndex)
    {
        *OutIndex = Confirmed_TroughSampleIndex;
    }
    if (OutValue)
    {
        *OutValue = Confirmed_TroughSampleValue;
    }
}

bool DetectBeep(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel)
{
    bool bResult = false;

    f32 CrossingThreshold = 0.05f;

    u32 Confirmations = 10;
    u32 NumCrossedZero = 0;
    u32 State = Idle;

    for (u32 i = 0; i < SearchLength; i++)
    {
        if (NumCrossedZero >= Confirmations)
        {
            bResult = true;
            break;
        }
        
        f32 Current = Samples[(SampleOffset+i) * Wav.channels + (bLeftChannel ? 0 : 1)];

        switch (State)
        {
            case Idle:
            {
                if (Current >= CrossingThreshold)
                {
                    State = Tracking;
                }
            }
            break;

            case Tracking:
            {
                if (Current <= -CrossingThreshold)
                {
                    NumCrossedZero++;
                    State = Idle;
                }
            }
            break;
        }
    }
    
    return bResult;
}

bool RecordPlayer_DecodeStep(f32* Samples, Wave Wav, RecordPlayer* Player, ImageMetaData ImageMetaData)
{
    bool bSuccess = false;

    u32 SamplesPerLine     = (f32)Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack              = 0.05 * (f32)SamplesPerLine;
    u32 NextLinePrediction = (Player->Cursor + (SamplesPerLine - Slack));

    i32 PeakIndex = -1;
    f32 PeakValue = 0;

    DetectScanTrigger(Samples, NextLinePrediction, Slack*2, Wav, Player->bLeftChannel, ImageMetaData.OverrideParams, &PeakIndex, &PeakValue);

    f32 BestScore = SyncPeak(Samples, Player->Cursor, SamplesPerLine, Wav.channels, Player->bLeftChannel) / SamplesPerLine;

    bool bIsBeep = DetectBeep(Samples, Player->Cursor, SamplesPerLine, Wav, Player->bLeftChannel);

    /*
    if (bIsBeep)
    {
        printf("Beep!\n");
    }
    */

    f32 Diff = fabsf(BestScore - Player->Threshold);
    bool bWithinBand = Diff < 0.1f;
    if (bWithinBand && !bIsBeep)
    {
        u32 NewOffset = NextLinePrediction + PeakIndex;

        u32 NextLineSamplesActual = NewOffset - Player->Cursor;

        f64 Black = -0.1;
        f64 White = 0.07;

        f64 ImageStart  = Player->Cursor;
        f64 ImageLen    = NextLineSamplesActual;

        i32 XOffset = ImageMetaData.bPortrait ? ImageScanHeight/8 : 0;

        if (Player->ScanLine < ImageScanWidth)
        {
            for (i32 y = 0; y < ImageScanHeight; y++)
            {
                u64 SampleIndex0 = (u64)ImageStart + ((f64)y     / (f64)ImageScanHeight) * ImageLen;
                u64 SampleIndex1 = (u64)ImageStart + ((f64)(y+1) / (f64)ImageScanHeight) * ImageLen;
                if (SampleIndex1 <= SampleIndex0) { SampleIndex1 = SampleIndex0 + 1; }
    
                f64 Sum = 0;
                for (u64 i = SampleIndex0; i < SampleIndex1; i++)
                {
                    Sum += Samples[i * Wav.channels + (Player->bLeftChannel ? 0 : 1)];
                }
                f64 Avg = Sum/(f64)(SampleIndex1-SampleIndex0);
    
                i32 V = (i32)((Avg - Black) / (White - Black) * 255.0);
                if (V < 0)
                {
                    V = 0;
                }
                else if (V > 255)
                {
                    V = 255;
                }
    
                V = 255 - V;
    
    
                i32 X = Player->ScanLine;
                i32 Y = y;
                if (ImageMetaData.bPortrait)
                {
                    if (ImageMetaData.bPortraitAntiClockwise)
                    {
                        X = XOffset + y;
                        Y = Player->ScanImage->height-Player->ScanLine;
                    }
                    else
                    {
                        X = XOffset + (ImageScanHeight-y);
                        Y = Player->ScanLine;
                    }
    
                    // clamp so its not out of bounds
                    if (X > Player->ScanImage->width-1)  { X = Player->ScanImage->width-1; }
                    if (Y > Player->ScanImage->height-1) { Y = Player->ScanImage->height-1; }
                    if (X < 0) { X = 0; }
                    if (Y < 0) { Y = 0; }
    
                    for (i32 i = 0; i < XOffset; i++)
                    {
                        ImageDrawPixel(Player->ScanImage, i, Y, BLACK);
                    }
        
                    for (i32 i = 0; i < Player->ScanImage->width-ImageScanHeight; i++)
                    {
                        ImageDrawPixel(Player->ScanImage, XOffset+ImageScanHeight+i, Y, BLACK);
                    }
                }
                else
                {
                    for (i32 i = 0; i < Player->ScanImage->height-ImageScanHeight; i++)
                    {
                        ImageDrawPixel(Player->ScanImage, Player->ScanLine, ImageScanHeight+i, BLACK);
                    }
                }
            
                Color CurrentColor = GetImageColor(*Player->ScanImage, X, Y);
                Color PixelColor = {0};
                switch (ImageMetaData.ColorChannel)
                {
                    default:
                    case Mono:
                    {
                        PixelColor = (Color){ V, V, V, 255 };
                    }
                    break;
    
                    case Red:
                    {
                        CurrentColor.r = V;
                        CurrentColor.g = 0;
                        CurrentColor.b = 0;
                        PixelColor = CurrentColor;
                    }
                    break;
    
                    case Green:
                    {
                        CurrentColor.g = V;
                        PixelColor = CurrentColor;
                    }
                    break;
    
                    case Blue:
                    {
                        CurrentColor.b = V;
                        PixelColor = CurrentColor;
                    }
                    break;
                }
    
                PixelColor.a = 255;
    
                ImageDrawPixel(Player->ScanImage, X, Y, PixelColor);
            }
        }

        Player->Cursor = NewOffset;
        bSuccess = true;
    }

    return bSuccess;
}

void SelectMapping(RecordPlayer* Left, RecordPlayer* Right, u32 Index,
                   f32* Samples, Wave Wav, Music GoldenWav)
{
    ImageMapping M = ImageMappings[Index];

    Left->ImageOffset = M.LeftImage.SampleOffset;
    Left->Cursor = M.LeftImage.SampleOffset;
    Left->ScanLine = 0;

    Right->ImageOffset = M.RightImage.SampleOffset;
    Right->Cursor = M.RightImage.SampleOffset;
    Right->ScanLine = 0;

    u32 SamplesPerLine = Wav.sampleRate * SAMPLES_FACTOR;

    Left->Threshold  = SyncPeak(Samples, Left->ImageOffset,  SamplesPerLine, Wav.channels, true);
    Right->Threshold = SyncPeak(Samples, Right->ImageOffset, SamplesPerLine, Wav.channels, false);

    u64 MusicPosition = M.LeftImage.SampleOffset < M.RightImage.SampleOffset ? M.LeftImage.SampleOffset : M.RightImage.SampleOffset;
    SeekMusicStream(GoldenWav, (f32)MusicPosition / (f32)Wav.sampleRate);
}

// long names like "Underwater scene with diver and fish" are wider than a column at the
// full title size, so step the size down until it fits.
i32 FitFontSize(Font TextFont, const char* Text, i32 MaxWidth, i32 MaxFontSize, i32 MinFontSize)
{
    i32 Result = MaxFontSize;

    while (Result > MinFontSize && MeasureTextEx(TextFont, Text, (f32)Result, FontSpacing).x > (f32)MaxWidth)
    {
        Result -= 2;
    }

    return Result;
}

// restart the reveal whenever the channel moves onto a different image
void UpdateTextReveal(TextReveal* Reveal, i32 MappingIndex, EImageColorChannel ColorChannel, f32 DeltaTime)
{
    if (Reveal->MappingIndex != MappingIndex)
    {
        Reveal->MappingIndex = MappingIndex;

        bool bLaterColorPass = (ColorChannel == Green) || (ColorChannel == Blue);

        Reveal->Elapsed = bLaterColorPass ? 1000.0f : 0.0f;
    }
    else
    {
        Reveal->Elapsed += DeltaTime;
    }
}

i32 TakeRevealedChars(i32* Budget, const char* Text)
{
    i32 Length = (i32)TextLength(Text);
    i32 Result = *Budget < Length ? *Budget : Length;

    *Budget -= Result;

    return Result;
}

void DrawRevealedText(Font TextFont, const char* Text, i32 CharCount, Vector2 Position, i32 FontSize, Color TextColor)
{
    const char* Revealed = TextSubtext(Text, 0, CharCount);
    DrawTextEx(TextFont, Revealed, Position, FontSize, FontSpacing, TextColor);

    if (CharCount > 0 && CharCount < (i32)TextLength(Text))
    {
        i32 CaretWidth = FontSize/10 < 2 ? 2 : FontSize/10;
        i32 CaretX     = (i32)(Position.x + MeasureTextEx(TextFont, Revealed, FontSize, FontSpacing).x);

        DrawRectangle(CaretX, (i32)Position.y, CaretWidth, FontSize, TextColor);
    }
}

const char* GetLocationText(GeoLocation Location)
{
    if (Location.Place == NULL)
    {
        return NULL;
    }

    return TextFormat("%s%s%.3f%c %.3f%c",
                      Location.Place,
                      Location.Place[0] ? "  " : "",
                      fabsf(Location.Latitude),  Location.Latitude  < 0.0f ? 'S' : 'N',
                      fabsf(Location.Longitude), Location.Longitude < 0.0f ? 'W' : 'E');
}

void DrawChannelMetaData(ImageMetaData Data, TextReveal Reveal, UILayout Layout, i32 ColumnX)
{
    i32 NameFontSize = FitFontSize(Fonts.Title, Data.Name, Layout.ColumnWidth, TitleFontSize, BodyFontSize);

    // bottom-align a shrunken title in its slot so the gap below stays constant
    i32 NameOffsetY = TitleFontSize - NameFontSize;

    const char* LocationText = GetLocationText(Data.Location);

    i32 Budget           = (i32)(Reveal.Elapsed * RevealCharsPerSecond);
    i32 NameChars        = TakeRevealedChars(&Budget, Data.Name);
    i32 SourceChars      = TakeRevealedChars(&Budget, Data.Source);
    i32 DescriptionChars = TakeRevealedChars(&Budget, Data.Description);
    i32 LocationChars    = TakeRevealedChars(&Budget, LocationText);

    DrawRevealedText(Fonts.Small, LocationText,     LocationChars,    (Vector2){ColumnX + Scaled(DesignCursorGutter), Layout.CursorY}, SmallFontSize, GRAY);
    DrawRevealedText(Fonts.Title, Data.Name,        NameChars,        (Vector2){ColumnX, Layout.NameY + NameOffsetY}, NameFontSize,  WHITE);
    DrawRevealedText(Fonts.Body,  Data.Source,      SourceChars,      (Vector2){ColumnX, Layout.SourceY},             BodyFontSize,  LIGHTGRAY);
    DrawRevealedText(Fonts.Body,  Data.Description, DescriptionChars, (Vector2){ColumnX, Layout.DescriptionY},        BodyFontSize,  GRAY);
}

const char* GetShortcutKeyLabel(ImageMapping Mapping)
{
    if (Mapping.ShiftKey != KEY_NULL)
    {
        return TextFormat("S-%c", (char)Mapping.ShiftKey);
    }

    if (Mapping.Key      != KEY_NULL)
    {
        return TextFormat("  %c", (char)Mapping.Key);
    }

    return "  -";
}

const char* GetShortcutImageNames(ImageMapping Mapping)
{
    return TextFormat("%s / %s",
                      Mapping.LeftImage.Name  ? Mapping.LeftImage.Name  : "",
                      Mapping.RightImage.Name ? Mapping.RightImage.Name : "");
}

// lists every image shortcut, and doubles as the way out of the app
void DrawShortcutMenu(Font TitleFont, Font RowFont, f32 EscapeHeld, i32 CurrentIndex)
{
    u32 NumMappings = sizeof(ImageMappings) / sizeof(ImageMappings[0]);

    const i32 MenuColumns  = 3;
    const i32 ColumnGap    = Scaled(30);
    const i32 PanelPadding = Scaled(30);
    const i32 KeyLabelChars = 5;   // "S-1" plus the two spaces separating it from the names

    i32 RowHeight     = MenuFontSize + Scaled(6);
    i32 RowsPerColumn = (NumMappings + MenuColumns - 1) / MenuColumns;

    // monospaced, so one glyph gives the advance for every row
    f32 CharWidth = MeasureTextEx(RowFont, "M", MenuFontSize, FontSpacing).x;

    i32 LongestChars = 0;
    for (u32 i = 0; i < NumMappings; i++)
    {
        i32 Chars = KeyLabelChars + (i32)TextLength(GetShortcutImageNames(ImageMappings[i]));
        if (Chars > LongestChars) LongestChars = Chars;
    }

    i32 ColumnWidth = (i32)(CharWidth * LongestChars);
    i32 TitleHeight = MenuTitleFontSize + Scaled(20);
    i32 FooterHeight = MenuFontSize + Scaled(24);

    i32 PanelWidth  = PanelPadding*2 + ColumnWidth*MenuColumns + ColumnGap*(MenuColumns-1);
    i32 PanelHeight = PanelPadding*2 + TitleHeight + RowsPerColumn*RowHeight + FooterHeight;

    i32 PanelX = GetScreenWidth()/2  - PanelWidth/2;
    i32 PanelY = GetScreenHeight()/2 - PanelHeight/2;

    DrawRectangle(PanelX, PanelY, PanelWidth, PanelHeight, Fade(BLACK, 0.93f));
    DrawRectangleLinesEx((Rectangle){PanelX, PanelY, PanelWidth, PanelHeight}, (f32)Scaled(2), Fade(WHITE, 0.35f));

    const char* MenuTitle = "IMAGE SHORTCUTS";
    DrawTextEx(TitleFont, MenuTitle,
               (Vector2){GetScreenWidth()*0.5f - MeasureTextEx(TitleFont, MenuTitle, MenuTitleFontSize, FontSpacing).x*0.5f, PanelY + PanelPadding},
               MenuTitleFontSize, FontSpacing, WHITE);

    i32 GridY = PanelY + PanelPadding + TitleHeight;
    for (u32 i = 0; i < NumMappings; i++)
    {
        i32 Column = i / RowsPerColumn;
        i32 Row    = i % RowsPerColumn;

        i32 RowX = PanelX + PanelPadding + Column*(ColumnWidth + ColumnGap);
        i32 RowY = GridY + Row*RowHeight;

        // the row the music cursor is sitting on, so the menu tells you where you are
        bool bIsCurrent = ((i32)i == CurrentIndex);
        if (bIsCurrent)
        {
            i32 HighlightPad = Scaled(6);
            DrawRectangle(RowX - HighlightPad, RowY - Scaled(3),
                          ColumnWidth + HighlightPad*2, RowHeight, Fade(WHITE, 0.2f));
        }

        DrawTextEx(RowFont, GetShortcutKeyLabel(ImageMappings[i]),   (Vector2){RowX, RowY}, MenuFontSize, FontSpacing, WHITE);
        DrawTextEx(RowFont, GetShortcutImageNames(ImageMappings[i]),
                   (Vector2){RowX + CharWidth*KeyLabelChars, RowY}, MenuFontSize, FontSpacing, bIsCurrent ? WHITE : GRAY);
    }

    // the hold-to-quit progress doubles as the prompt telling you it exists
    i32 FooterY = PanelY + PanelHeight - PanelPadding - MenuFontSize;
    const char* FooterText = "TAP ESC TO CLOSE    HOLD ESC TO QUIT    C-F FULLSCREEN    C-R REPLAY INTRO    C-LEFT/C-RIGHT WAVEFORMS    LEFT/RIGHT STEP IMAGES";
    DrawTextEx(RowFont, FooterText,
               (Vector2){GetScreenWidth()*0.5f - MeasureTextEx(RowFont, FooterText, MenuFontSize, FontSpacing).x*0.5f, FooterY},
               MenuFontSize, FontSpacing, GRAY);

    f32 HoldProgress = EscapeHeld / MenuExitHoldSeconds;
    if (HoldProgress > 1.0f) HoldProgress = 1.0f;

    i32 BarWidth  = PanelWidth - PanelPadding*2;
    i32 BarHeight = Scaled(3);
    i32 BarX      = PanelX + PanelPadding;
    i32 BarY      = FooterY + MenuFontSize + Scaled(8);

    DrawRectangle(BarX, BarY, BarWidth, BarHeight, Fade(WHITE, 0.15f));
    DrawRectangle(BarX, BarY, (i32)(BarWidth * HoldProgress), BarHeight, WHITE);
}

void DrawChannelWaveform(f32* Samples, f32 MusicCursor, bool bLeftChannel,
                         i32 BaseLocationX, i32 BaseLocationY, i32 ScanWidth, i32 ScanHeight)
{
    u32 SamplesPerLine = GoldenWav.sampleRate * SAMPLES_FACTOR;

    if (MusicCursor <= SamplesPerLine * NumScanlinesToDraw)
    {
        return;
    }

    u32 Channel = bLeftChannel ? 0 : 1;

    u32 Draw_StartPointX = BaseLocationX;
    u32 Draw_EndPointX   = BaseLocationX + (ScanWidth  * ImageScale - Scaled(30));
    u32 Draw_MidPointY   = BaseLocationY + (ScanHeight * ImageScale + Scaled(85));

    u32 Window      = SamplesPerLine * NumScanlinesToDraw;
    u32 CursorStart = MusicCursor - Window;
    u32 CursorEnd   = MusicCursor;

    u32 Slack = 0.05 * SamplesPerLine;
    i32 TriggerRel = 0;
    DetectScanTrigger(Samples, CursorStart, Slack, GoldenWav, bLeftChannel, NULL, &TriggerRel, NULL);

    if (TriggerRel > 0)
    {
        CursorStart = CursorStart + TriggerRel;
        Window      = CursorEnd - CursorStart;
    }

    for (u32 i = CursorStart; i < CursorEnd; i++)
    {
        f32 Value = Samples[i * GoldenWav.channels + Channel] * 350.0f * UIScale;

        f32 Alpha = ((f32)(CursorEnd - i) / Window);

        u32 XOffset = Draw_StartPointX + (i32)((Draw_EndPointX - Draw_StartPointX) * (1.0f - Alpha));

        DrawRectangle(XOffset, Draw_MidPointY + Value, Scaled(1), Scaled(1), WHITE);
    }
}

bool UpdateLayoutScale(void)
{
    f32 WidthScale  = (f32)GetScreenWidth()  / (f32)DesignWidth;
    f32 HeightScale = (f32)GetScreenHeight() / (f32)DesignHeight;

    // the narrower axis wins, so nothing spills off a window that is not 16:9
    f32 NewScale = WidthScale < HeightScale ? WidthScale : HeightScale;

    if (NewScale == UIScale)
    {
        return false;
    }

    UIScale = NewScale;

    LeftOffset = Scaled(DesignLeftOffset);
    ImageScale = DesignImageScale * UIScale;

    i32 PreviousTitleFontSize     = TitleFontSize;
    i32 PreviousBodyFontSize      = BodyFontSize;
    i32 PreviousSmallFontSize     = SmallFontSize;
    i32 PreviousMenuFontSize      = MenuFontSize;
    i32 PreviousMenuTitleFontSize = MenuTitleFontSize;

    TitleFontSize     = Scaled(DesignTitleFontSize);
    BodyFontSize      = Scaled(DesignBodyFontSize);
    SmallFontSize     = Scaled(DesignSmallFontSize);
    MenuFontSize      = Scaled(DesignMenuFontSize);
    MenuTitleFontSize = Scaled(DesignMenuTitleFontSize);

    return TitleFontSize     != PreviousTitleFontSize
        || BodyFontSize      != PreviousBodyFontSize
        || SmallFontSize     != PreviousSmallFontSize
        || MenuFontSize      != PreviousMenuFontSize
        || MenuTitleFontSize != PreviousMenuTitleFontSize;
}

UILayout GetUILayout(void)
{
    const i32 LeftPadding = Scaled(0);

    UILayout Result = {0};

    Result.BaseLocationX = GetScreenWidth()/2  - LeftOffset;
    Result.BaseLocationY = GetScreenHeight()/2 - Scaled(375);

    Result.ColumnWidth   = LeftOffset - LeftPadding*2;
    Result.LeftColumnX   = Result.BaseLocationX + LeftPadding;
    Result.RightColumnX  = Result.BaseLocationX + LeftOffset + LeftPadding;

    Result.DescriptionY  = Result.BaseLocationY - BodyFontSize  - Scaled(9);
    Result.SourceY       = Result.DescriptionY  - BodyFontSize  - Scaled(2);
    Result.NameY         = Result.SourceY       - TitleFontSize - Scaled(4);
    Result.CursorY       = Result.NameY         - SmallFontSize - Scaled(4);

    return Result;
}

void LoadUIFonts(UIFonts* Fonts)
{
    f32 PixelDensity = GetWindowScaleDPI().x;
    if (PixelDensity < 1.0f)
    {
        PixelDensity = 1.0f;
    }

    Fonts->Title = LoadFontEx("Resources/IBM_Plex_Mono/IBMPlexMono-Bold.ttf",    (i32)(TitleFontSize    * PixelDensity), NULL, 0);
    Fonts->Body  = LoadFontEx("Resources/IBM_Plex_Mono/IBMPlexMono-Regular.ttf", (i32)(BodyFontSize     * PixelDensity), NULL, 0);
    Fonts->Small = LoadFontEx("Resources/IBM_Plex_Mono/IBMPlexMono-Regular.ttf", (i32)(SmallFontSize    * PixelDensity), NULL, 0);
    Fonts->Menu  = LoadFontEx("Resources/IBM_Plex_Mono/IBMPlexMono-Regular.ttf", (i32)(MenuFontSize     * PixelDensity), NULL, 0);

    SetTextureFilter(Fonts->Title.texture, TEXTURE_FILTER_BILINEAR);
}

void UnloadUIFonts(UIFonts* Fonts)
{
    UnloadFont(Fonts->Title);
    UnloadFont(Fonts->Body);
    UnloadFont(Fonts->Small);
    UnloadFont(Fonts->Menu);
}

void FitWindowToMonitor(void)
{
    i32 Monitor = GetCurrentMonitor();

    i32 MonitorWidth  = GetMonitorWidth(Monitor);
    i32 MonitorHeight = GetMonitorHeight(Monitor);

    // a headless or otherwise unreported monitor leaves the requested size alone
    if (MonitorWidth <= 0 || MonitorHeight <= 0)
    {
        return;
    }

    f32 WidthFit  = (MonitorWidth  * WindowScreenMargin) / (f32)DesignWidth;
    f32 HeightFit = (MonitorHeight * WindowScreenMargin) / (f32)DesignHeight;

    // the tighter axis wins so the aspect ratio holds and the layout scales uniformly
    f32 Fit = WidthFit < HeightFit ? WidthFit : HeightFit;

    // never blow the window up past the design size on a larger monitor
    if (Fit > 1.0f)
    {
        Fit = 1.0f;
    }

    i32 Width  = (i32)(DesignWidth  * Fit);
    i32 Height = (i32)(DesignHeight * Fit);

    // the minimum size still wins on a very small panel, even if it overflows the margin
    if (Width < MinWindowWidth)
    {
        Width = MinWindowWidth;
    }

    if (Height < MinWindowHeight)
    {
        Height = MinWindowHeight;
    }

    SetWindowSize(Width, Height);

    Vector2 MonitorPosition = GetMonitorPosition(Monitor);

    i32 PositionX = (i32)MonitorPosition.x + (MonitorWidth  - Width)  / 2;
    i32 PositionY = (i32)MonitorPosition.y + (MonitorHeight - Height) / 2;

    if (PositionX < (i32)MonitorPosition.x)
    {
        PositionX = (i32)MonitorPosition.x;
    }

    // centring puts the title bar off the top of a screen the window barely fits on
    if (PositionY < (i32)MonitorPosition.y + WindowTopMargin)
    {
        PositionY = (i32)MonitorPosition.y + WindowTopMargin;
    }

    SetWindowPosition(PositionX, PositionY);
}

#if defined(__linux__)
void SetWindowIconFromFile(const char* FileName)
{
    const i32 IconSizes[] = {16, 24, 32, 48, 64, 128, 256};
    const i32 IconCount   = 7;

    Image Source = LoadImage(FileName);
    if (!IsImageValid(Source))
    {
        TraceLog(LOG_WARNING, "%s is missing, keeping the default window icon.", FileName);
        return;
    }

    // SetWindowIcons only accepts RGBA32, it will not convert for us
    ImageFormat(&Source, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Image Icons[7] = {0};
    for (i32 i = 0; i < IconCount; i++)
    {
        Icons[i] = ImageCopy(Source);
        ImageResize(&Icons[i], IconSizes[i], IconSizes[i]);
    }

    SetWindowIcons(Icons, IconCount);

    // the icons are copied into the platform layer, so the originals are ours to free
    for (i32 i = 0; i < IconCount; i++)
    {
        UnloadImage(Icons[i]);
    }

    UnloadImage(Source);
}
#endif

void RecordPlayer_Update(f32* Samples, RecordPlayer* Player)
{
    u32 SamplesPerLine = GoldenWav.sampleRate * SAMPLES_FACTOR;

    f32 Peak = SyncPeak(Samples, Player->Cursor, SamplesPerLine, GoldenWav.channels, Player->bLeftChannel);
    Player->Threshold = Peak/SamplesPerLine;

    while (Player->Cursor <= MusicCursor)
    {
        ImageMetaData MetaData = GetImageMetaDataFromSampleOffset(Player->Cursor, Player->bLeftChannel);

        if (RecordPlayer_DecodeStep(Samples, GoldenWav, Player, MetaData))
        {
            Player->ScanLine++;
        }
        else
        {
            Player->ScanLine = 0;
            Player->Cursor = MusicCursor;
            break;
        }
    }

    UpdateTexture(Player->ScanTexture, Player->ScanImage->data);

    Player->ChannelIndex = GetChannelIndexFromSampleOffset(Player->Cursor, Player->bLeftChannel);
    Player->MetaData     = GetImageMetaDataFromSampleOffset(Player->Cursor, Player->bLeftChannel);

    f32 RevealDelta = bPaused ? 0.0f : GetFrameTime();
    UpdateTextReveal(&Player->RevealState, Player->ChannelIndex, Player->MetaData.ColorChannel, RevealDelta);
}

Rectangle GetChannelImageBounds(RecordPlayer* Player, UILayout Layout)
{
    i32 ImageX = Player->bLeftChannel ? Layout.BaseLocationX : Layout.BaseLocationX + LeftOffset;

    f32 Width = ImageScanWidth * ImageScale;

    if (Player->bLeftChannel && Width > (f32)LeftOffset)
    {
        Width = (f32)LeftOffset;
    }

    return (Rectangle){(f32)ImageX,
                       (f32)Layout.BaseLocationY,
                       Width,
                       ImageScanHeight * ImageScale};
}

void RecordPlayer_Draw(RecordPlayer* Player, UILayout Layout)
{
    Rectangle ImageBounds = GetChannelImageBounds(Player, Layout);

    i32 ImageX  = (i32)ImageBounds.x;
    i32 ColumnX = Player->bLeftChannel ? Layout.LeftColumnX : Layout.RightColumnX;

    DrawTextureEx(Player->ScanTexture, (Vector2){ImageBounds.x, ImageBounds.y}, 0, ImageScale, WHITE);

    DrawChannelMetaData(Player->MetaData, Player->RevealState, Layout, ColumnX);

    // current decode cursor for the channel
    DrawTextEx(Fonts.Small, TextFormat("%s %u", Player->bLeftChannel ? "L" : "R", Player->Cursor),
               (Vector2){ColumnX, Layout.CursorY}, SmallFontSize, FontSpacing, GRAY);

    if (Player->bDrawWaveform)
    {
        DrawChannelWaveform(GoldenSamples, Player->Cursor,
                            Player->bLeftChannel, ImageX, Layout.BaseLocationY,
                            ImageScanWidth, ImageScanHeight);
    }
}

void Init(void)
{
    ChangeDirectory(GetApplicationDirectory());
    
    #if defined(__APPLE__)
    if (DirectoryExists("../Resources/Resources"))
    {
        ChangeDirectory("../Resources");
    }
    #endif
    
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(DesignWidth, DesignHeight, "Golden Decoder");
    
    #if defined(__linux__)
    SetWindowIconFromFile("Resources/Voyager_Golden_Record.png");
    #endif
    
    SetWindowMinSize(MinWindowWidth, MinWindowHeight);
    FitWindowToMonitor();
    SetTargetFPS(0);
    
    // escape opens the menu instead of closing the window out from under us
    SetExitKey(KEY_NULL);
    
    InitAudioDevice();
}

void ResetRecordPlayer(RecordPlayer* Player)
{
    Player->ImageOffset  = 0;
    Player->Cursor       = 0;
    Player->ScanLine     = 0;
    Player->Threshold    = 0.0f;
    Player->ChannelIndex = -1;
    Player->MetaData     = (ImageMetaData){0};
    Player->RevealState  = (TextReveal){.MappingIndex = -1};

    ImageClearBackground(Player->ScanImage, BLANK);
    UpdateTexture(Player->ScanTexture, Player->ScanImage->data);
}

// the decoders follow the music cursor, so a rewind of the track has to rewind them too
void RestartIntro(void)
{
    bIntroActive = true;
    IntroElapsed = 0.0f;
    bMenuOpen    = false;
    bPaused      = false;

    StopMusicStream(GoldenMusic);

    ResetRecordPlayer(&Player_LeftChannel);
    ResetRecordPlayer(&Player_RightChannel);
}

// the control chords work in both stages, and they never count as a skip of the intro
bool UpdateControlChordInput(void)
{
    if (!IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL))
    {
        return false;
    }

    if (IsKeyPressed(KEY_F))
    {
        ToggleBorderlessWindowed();
    }

    if (IsKeyPressed(KEY_R))
    {
        RestartIntro();
    }

    if (IsKeyPressed(KEY_LEFT))
    {
        Player_LeftChannel.bDrawWaveform = !Player_LeftChannel.bDrawWaveform;
    }

    if (IsKeyPressed(KEY_RIGHT))
    {
        Player_RightChannel.bDrawWaveform = !Player_RightChannel.bDrawWaveform;
    }

    return true;
}

f32 GetIntroTypingSeconds(void)
{
    u32 NumLines = sizeof(IntroQuoteLines) / sizeof(IntroQuoteLines[0]);

    i32 TotalChars = (i32)TextLength(IntroAttribution);
    for (u32 i = 0; i < NumLines; i++)
    {
        TotalChars += (i32)TextLength(IntroQuoteLines[i]);
    }

    return (f32)TotalChars / IntroRevealCharsPerSecond;
}

f32 GetIntroFadeStartSeconds(void)
{
    return GetIntroTypingSeconds() + IntroHoldSeconds;
}

f32 GetIntroAlpha(void)
{
    f32 FadeStart = GetIntroFadeStartSeconds();

    if (IntroElapsed <= FadeStart)
    {
        return 1.0f;
    }

    f32 Result = 1.0f - (IntroElapsed - FadeStart) / IntroFadeSeconds;

    if (Result < 0.0f)
    {
        Result = 0.0f;
    }

    return Result;
}

void EndIntro(void)
{
    bIntroActive = false;

    PlayMusicStream(GoldenMusic);
}

void UpdateIntro(void)
{
    IntroElapsed += GetFrameTime();

    if (UpdateControlChordInput())
    {
        return;
    }

    bool bSkipped = (GetKeyPressed() != KEY_NULL);

    if (bSkipped || IntroElapsed >= GetIntroFadeStartSeconds() + IntroFadeSeconds)
    {
        EndIntro();
    }
}

void DrawIntro(void)
{
    u32 NumLines = sizeof(IntroQuoteLines) / sizeof(IntroQuoteLines[0]);

    f32 Alpha = GetIntroAlpha();

    i32 MaxLineWidth = (i32)(GetScreenWidth() * IntroQuoteWidthFraction);

    // one size for every line, so the block reads as a single piece of text
    i32 QuoteFontSize = TitleFontSize;
    for (u32 i = 0; i < NumLines; i++)
    {
        i32 LineFontSize = FitFontSize(Fonts.Title, IntroQuoteLines[i], MaxLineWidth, TitleFontSize, SmallFontSize);
        if (LineFontSize < QuoteFontSize) QuoteFontSize = LineFontSize;
    }

    i32 LineGap         = Scaled(DesignQuoteLineGap);
    i32 AttributionGap  = Scaled(DesignQuoteAttributionGap);

    i32 QuoteHeight = NumLines*QuoteFontSize + (NumLines-1)*LineGap;
    i32 BlockHeight = QuoteHeight + AttributionGap + BodyFontSize;
    i32 BlockY      = GetScreenHeight()/2 - BlockHeight/2;

    i32 Budget = (i32)(IntroElapsed * IntroRevealCharsPerSecond);

    for (u32 i = 0; i < NumLines; i++)
    {
        const char* Line = IntroQuoteLines[i];

        i32 LineChars = TakeRevealedChars(&Budget, Line);
        f32 LineWidth = MeasureTextEx(Fonts.Title, Line, (f32)QuoteFontSize, FontSpacing).x;

        DrawRevealedText(Fonts.Title, Line, LineChars,
                         (Vector2){GetScreenWidth()*0.5f - LineWidth*0.5f, (f32)(BlockY + i*(QuoteFontSize + LineGap))},
                         QuoteFontSize, Fade(WHITE, Alpha));
    }

    i32 AttributionChars = TakeRevealedChars(&Budget, IntroAttribution);
    f32 AttributionWidth = MeasureTextEx(Fonts.Body, IntroAttribution, (f32)BodyFontSize, FontSpacing).x;

    DrawRevealedText(Fonts.Body, IntroAttribution, AttributionChars,
                     (Vector2){GetScreenWidth()*0.5f - AttributionWidth*0.5f, (f32)(BlockY + QuoteHeight + AttributionGap)},
                     BodyFontSize, Fade(GRAY, Alpha));

    // the prompt holds off for a moment so it does not compete with the opening line
    f32 HintAlpha = (IntroElapsed - IntroHintDelaySeconds) / IntroHintFadeSeconds;

    if (HintAlpha > 1.0f) HintAlpha = 1.0f;

    if (HintAlpha > 0.0f)
    {
        const char* HintText = "TAP ANY KEY TO SKIP";

        DrawTextEx(Fonts.Menu, HintText,
                   (Vector2){GetScreenWidth()*0.5f - MeasureTextEx(Fonts.Menu, HintText, (f32)MenuFontSize, FontSpacing).x*0.5f,
                             (f32)(GetScreenHeight() - Scaled(DesignQuoteHintMargin))},
                   MenuFontSize, FontSpacing, Fade(WHITE, 0.4f * HintAlpha * Alpha));
    }
}

void UpdatePlaybackInput(void)
{
    if (IsKeyPressed(KEY_SPACE))
    {
        bPaused = !bPaused;

        if (bPaused)
        {
            PauseMusicStream(GoldenMusic);
        }
        else
        {
            ResumeMusicStream(GoldenMusic);
        }
    }
}

void UpdateLayoutAndFonts(void)
{
    if (UpdateLayoutScale())
    {
        UnloadUIFonts(&Fonts);
        LoadUIFonts(&Fonts);
    }
}

void UpdateMenuInput(void)
{
    if (!bMenuOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            bMenuOpen        = true;
            bMenuEscapeArmed = false;
            MenuEscapeHeld   = 0.0f;
        }
    }
    else if (!bMenuEscapeArmed)
    {
        bMenuEscapeArmed = IsKeyUp(KEY_ESCAPE);
    }
    else if (IsKeyDown(KEY_ESCAPE))
    {
        MenuEscapeHeld += GetFrameTime();
        bQuitRequested = MenuEscapeHeld >= MenuExitHoldSeconds;
    }
    else if (MenuEscapeHeld > 0.0f)
    {
        bMenuOpen      = false;
        MenuEscapeHeld = 0.0f;
    }
}

void UpdateImageSelectionInput(void)
{
    u32 NumMappings = sizeof(ImageMappings) / sizeof(ImageMappings[0]);

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))
    {
        i32 Step   = IsKeyPressed(KEY_RIGHT) ? 1 : -1;
        i32 Found = GetChannelIndexFromSampleOffset(Player_LeftChannel.Cursor, true);
        i32 Target = -1;
        if (Found >= 0)
        {
            Target = Found + Step;
        }

        if (Target >= 0 && Target < (i32)NumMappings)
        {
            SelectMapping(&Player_LeftChannel, &Player_RightChannel, Target, GoldenSamples, GoldenWav, GoldenMusic);
        }
    }

    bool bIsShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    for (u32 i = 0; i < NumMappings; i++)
    {
        ImageMapping M = ImageMappings[i];

        if (IsKeyPressed(bIsShift ? M.ShiftKey : M.Key))
        {
            SelectMapping(&Player_LeftChannel, &Player_RightChannel, i, GoldenSamples, GoldenWav, GoldenMusic);

            bMenuOpen = false;
            break;
        }
    }
}

void UpdateShortcutInput(void)
{
    if (UpdateControlChordInput())
    {
        return;
    }

    UpdateImageSelectionInput();

    if (IsKeyPressed(KEY_UP))
    {
        NumScanlinesToDraw++;
    }

    if (IsKeyPressed(KEY_DOWN))
    {
        NumScanlinesToDraw--;
    }
}

RecordPlayer* GetHoveredSourceLink(UILayout Layout)
{
    if (bMenuOpen)
    {
        return NULL;
    }

    RecordPlayer* Players[2] = {&Player_LeftChannel, &Player_RightChannel};

    Vector2 Mouse = GetMousePosition();

    for (u32 i = 0; i < 2; i++)
    {
        if (Players[i]->MetaData.SourceURL == NULL)
        {
            continue;
        }

        if (CheckCollisionPointRec(Mouse, GetChannelImageBounds(Players[i], Layout)))
        {
            return Players[i];
        }
    }

    return NULL;
}

void UpdateSourceLinkInput(void)
{
    RecordPlayer* Hovered = GetHoveredSourceLink(GetUILayout());

    // SetMouseCursor builds a fresh cursor object per call, so only touch it on a change
    static i32 AppliedCursor = MOUSE_CURSOR_DEFAULT;

    i32 WantedCursor = Hovered ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT;

    if (WantedCursor != AppliedCursor)
    {
        SetMouseCursor(WantedCursor);
        AppliedCursor = WantedCursor;
    }

    if (Hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        OpenURL(Hovered->MetaData.SourceURL);
    }
}

void Update(void)
{
    UpdateLayoutAndFonts();

    if (bIntroActive)
    {
        UpdateIntro();
        return;
    }

    UpdateMusicStream(GoldenMusic);

    UpdatePlaybackInput();

    UpdateMenuInput();
    UpdateShortcutInput();

    MusicCursor = (f32)GetMusicTimePlayed(GoldenMusic) * (f32)GoldenWav.sampleRate;

    RecordPlayer_Update(GoldenSamples, &Player_LeftChannel);
    RecordPlayer_Update(GoldenSamples, &Player_RightChannel);

    UpdateSourceLinkInput();
}

void DrawRecordProgress(void)
{
    const i32 BarHeight = Scaled(4);

    f32 TrackLength = GetMusicTimeLength(GoldenMusic);
    f32 Progress    = TrackLength > 0.0f ? GetMusicTimePlayed(GoldenMusic) / TrackLength : 0.0f;

    if (Progress > 1.0f)
    {
        Progress = 1.0f;
    }

    DrawRectangle(0, 0, GetScreenWidth(), BarHeight, Fade(WHITE, 0.15f));
    DrawRectangle(0, 0, (i32)(GetScreenWidth() * Progress), BarHeight, WHITE);
}

void DrawPlaybackTime(UILayout Layout)
{
    f32 PlayedTime = GetMusicTimePlayed(GoldenMusic);
    const char* TimeText = TextFormat("%02i:%02i:%03i", (i32)PlayedTime / 60, (i32)PlayedTime % 60, (i32)(PlayedTime * 1000) % 1000);

    i32 GlyphSize = Scaled(14);
    i32 GlyphGap  = Scaled(12);

    i32 GroupWidth = GlyphSize + GlyphGap + (i32)MeasureTextEx(Fonts.Body, TimeText, BodyFontSize, FontSpacing).x;
    i32 GroupX     = GetScreenWidth() - GroupWidth;
    i32 GroupY     = Layout.BaseLocationY - Scaled(160);

    // the glyph shows the state at a glance, and blinks while paused
    f32 GlyphAlpha = bPaused ? 0.25f + 0.35f * (0.5f + 0.5f * sinf((f32)GetTime() * 2.5f)) : 1.0f;
    Color GlyphColor = Fade(WHITE, GlyphAlpha);

    i32 GlyphY = GroupY + (BodyFontSize - GlyphSize)/2;
    if (bPaused)
    {
        i32 BarWidth = GlyphSize/3;
        DrawRectangle(GroupX, GlyphY, BarWidth, GlyphSize, GlyphColor);
        DrawRectangle(GroupX + GlyphSize - BarWidth, GlyphY, BarWidth, GlyphSize, GlyphColor);
    }

    DrawTextEx(Fonts.Body, TimeText, (Vector2){GroupX + GlyphSize + GlyphGap, GroupY}, BodyFontSize, FontSpacing, GlyphColor);
}

void Draw(void)
{
    if (bIntroActive)
    {
        DrawIntro();
        return;
    }

    UILayout Layout = GetUILayout();

    DrawRecordProgress();
    DrawPlaybackTime(Layout);

    RecordPlayer_Draw(&Player_LeftChannel,  Layout);
    RecordPlayer_Draw(&Player_RightChannel, Layout);

    if (bMenuOpen)
    {
        DrawShortcutMenu(Fonts.Title, Fonts.Menu, MenuEscapeHeld, Player_LeftChannel.ChannelIndex);
    }
}

i32 main(void)
{
    Init();

    GoldenMusic = LoadMusicStream("Resources/golden.wav");
    GoldenWav = LoadWave("Resources/golden.wav");

    bool bHaveWave  = IsWaveValid(GoldenWav);
    bool bHaveMusic = IsMusicValid(GoldenMusic);

    GoldenSamples = bHaveWave ? LoadWaveSamples(GoldenWav) : NULL;

    // nothing reads the raw 16-bit data after the f32 conversion, only sampleRate, channels and frameCount
    UnloadWave(GoldenWav);
    GoldenWav.data = NULL;

    if (!bHaveMusic || !bHaveWave || GoldenSamples == NULL)
    {
        TraceLog(LOG_ERROR, "Resources/golden.wav is missing or is not a readable wav file.");
        TraceLog(LOG_ERROR, "if this is a fresh clone, the audio is stored in git lfs: install it and run \"git lfs pull\".");

        UnloadWaveSamples(GoldenSamples);
        UnloadMusicStream(GoldenMusic);
        CloseAudioDevice();
        CloseWindow();

        return 1;
    }

    Image Scan_Left = GenImageColor(ImageCanvasScanWidth, ImageCanvasScanHeight, BLANK);
    Texture2D ScanTexture_Left = LoadTextureFromImage(Scan_Left);
    
    Image Scan_Right = GenImageColor(ImageCanvasScanWidth, ImageCanvasScanHeight, BLANK);
    Texture2D ScanTexture_Right = LoadTextureFromImage(Scan_Right);

    // turn this off to see the raw pixels without any filtering
    SetTextureFilter(ScanTexture_Right, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ScanTexture_Left, TEXTURE_FILTER_BILINEAR);

    UpdateLayoutScale();
    LoadUIFonts(&Fonts);

    Player_LeftChannel  = (RecordPlayer){.ScanImage = &Scan_Left,  .ScanTexture = ScanTexture_Left,  .bLeftChannel = true,  .bDrawWaveform = true, .RevealState = (TextReveal){.MappingIndex = -1}};
    Player_RightChannel = (RecordPlayer){.ScanImage = &Scan_Right, .ScanTexture = ScanTexture_Right, .bLeftChannel = false, .bDrawWaveform = true, .RevealState = (TextReveal){.MappingIndex = -1}};

    while (!WindowShouldClose() && !bQuitRequested)
    {
        Update();

        BeginDrawing();
        ClearBackground(BLACK);
        Draw();
        EndDrawing();
    }

    UnloadUIFonts(&Fonts);

    UnloadTexture(ScanTexture_Left);
    UnloadTexture(ScanTexture_Right);
    UnloadImage(Scan_Left);
    UnloadImage(Scan_Right);
    UnloadWaveSamples(GoldenSamples);
    UnloadMusicStream(GoldenMusic);

    CloseWindow();

    return 0;
}
