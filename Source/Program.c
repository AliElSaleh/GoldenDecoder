#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// Unsigned integer types
typedef unsigned char        u8;
typedef unsigned short       u16;
typedef unsigned int         u32;
typedef unsigned long long   u64;

// Signed integer types
typedef signed char          i8;
typedef signed short         i16;
typedef signed int           i32;
typedef signed long long     i64;

// floating-point types
typedef float                f32;
typedef double               f64;

const i32 DesignWidth  = 1920;
const i32 DesignHeight = 1080;

const i32 MinWindowWidth  = 854;
const i32 MinWindowHeight = 480;

const f32 WindowScreenMargin = 0.9f;
const i32 WindowTopMargin = 60;

const i32 DesignLeftOffset = 950;

const f32 DesignImageScale = 1.8f;

const i32 DesignTitleFontSize = 65;
const i32 DesignBodyFontSize  = 20;
const i32 DesignSmallFontSize = 18;
// 20 is about the practical ceiling, the three columns reach 1659px of the 1920 wide screen
const i32 DesignMenuFontSize      = 20;
const i32 DesignMenuTitleFontSize = 30;

const u32 LineHeight = 430;

// live values for the current window size. zero scale forces the first UpdateLayoutScale to build them
f32 UIScale = 0.0f;

i32 LeftOffset;

f32 ImageScale;

i32 TitleFontSize;
i32 BodyFontSize;
i32 SmallFontSize;
i32 MenuFontSize;
i32 MenuTitleFontSize;

const f32 FontSpacing = 0.0f;

i32 Scaled(i32 DesignPixels)
{
    i32 Result = (i32)(DesignPixels * UIScale + 0.5f);

    // a hairline still has to be visible when the window is smaller than the design size
    if (DesignPixels > 0 && Result < 1)
    {
        Result = 1;
    }

    return Result;
}

const f32 RevealCharsPerSecond = 15.0f;
const f32 RevealInstantSeconds = 1000.0f;

const f32 MenuExitHoldSeconds = 1.0f;

typedef struct
{
    Font Title;
    Font Body;
    Font Small;
    Font Menu;
} UIFonts;

typedef u8 EImageColorChannel;
enum
{
    Mono  = 0,
    Red   = 1,
    Green = 2,
    Blue  = 3
};

typedef u8 EImageSourceType;
enum
{
    Unknown    = 0,
    Photograph = 1,
    Book       = 2,
    Event      = 3,
};

typedef struct
{
    f32 FallThreshold;
    u32 DebounceSamples;
} ScanTriggerThresholdParams;


typedef struct
{
    const char* Name;
    const char* Source;
    const char* Description;
    EImageColorChannel ColorChannel;
    EImageSourceType SourceType; // TODO
    u32 SampleOffset;
    ScanTriggerThresholdParams* OverrideParams;
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
} RecordPlayer;


#define SAMPLES_FACTOR (1.0f/(60.0f))

// TODO: slow mo scanline slider, so you can see the vertical pixel lines filling in realtime
// TODO: clickable web links to go to original source and an auxiliary links
// TODO: quote at beginning of the experience. tap any key to skip to the decoding
// TODO: google maps coordinates of where pictures were taken
// TODO: export to jpeg

// names referenced from: 
//    https://science.nasa.gov/mission/voyager/golden-record-contents/images/
//    https://en.wikipedia.org/wiki/Contents_of_the_Voyager_Golden_Record

// no published credit exists for The Sun or Heron Island (record 41):
// both credit cells are blank on Wikipedia and neither has a NASA image-detail page.

// yes i really did type this by hand
ImageMapping ImageMappings[] =
{
    {.Key = KEY_ONE,         .LeftImage.Name = "Calibration Circle",                    .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 1375283,  .RightImage.SampleOffset = 1490087,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "David Doubilet"},
    {.Key = KEY_TWO,         .LeftImage.Name = "Solar Location in the Milky Way",       .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 1905722,  .RightImage.SampleOffset = 1984822,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Frank Drake", .RightImage.Source = "David Doubilet", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.0275f, .DebounceSamples = 3}},
    {.Key = KEY_THREE,       .LeftImage.Name = "Math Definitions",                      .RightImage.Name = "School of Fish",                            .LeftImage.SampleOffset = 2446590,  .RightImage.SampleOffset = 2505754,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "David Doubilet"},
    {.Key = KEY_FOUR,        .LeftImage.Name = "Physics Definitions",                   .RightImage.Name = "Tree Toad",                                 .LeftImage.SampleOffset = 2968481,  .RightImage.SampleOffset = 3033022,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Dave Wickstrom", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.03f, .DebounceSamples = 2}},
    {.Key = KEY_FIVE,        .LeftImage.Name = "Solar System Parameters 1",             .RightImage.Name = "Crocodile",                                 .LeftImage.SampleOffset = 3483550,  .RightImage.SampleOffset = 3546161,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Peter Beard"},
    {.Key = KEY_SIX,         .LeftImage.Name = "Solar System Parameters 2",             .RightImage.Name = "Eagle",                                     .LeftImage.SampleOffset = 3998630,  .RightImage.SampleOffset = 4051738,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Donona, Taplinger Publishing Co."},
    {.Key = KEY_SEVEN,       .LeftImage.Name = "The Sun",                               .RightImage.Name = "Zebras",                                    .LeftImage.SampleOffset = 4500698,  .RightImage.SampleOffset = 4544028,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Description = "Hale Observations", .RightImage.Source = "South African Tourist Corp."},
    {.Key = KEY_EIGHT,       .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 4984064,  .RightImage.SampleOffset = 5036615,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Red,   .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall"},
    {.Key = KEY_NINE,        .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 5467791,  .RightImage.SampleOffset = 5543004,  .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Green, .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall"},
    {.Key = KEY_NULL,        .LeftImage.Name = "Solar Spectrum",                        .RightImage.Name = "Jane Goodall & Chimps",                     .LeftImage.SampleOffset = 5955425,  .RightImage.SampleOffset = 6039902,  .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Source = "Vanne Morris-Goodall"},
    {.Key = KEY_ZERO,        .LeftImage.Name = "Mercury",                               .RightImage.Name = "Sketch of Bushmen Hunters",                 .LeftImage.SampleOffset = 6582824,  .RightImage.SampleOffset = 6570356,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "Jon Lomberg"},
    {.Key = KEY_Q,           .LeftImage.Name = "Mars",                                  .RightImage.Name = "Bushmen Hunters",                           .LeftImage.SampleOffset = 7091405,  .RightImage.SampleOffset = 7067636,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "R. Farbman; Time Inc."},
    {.Key = KEY_W,           .LeftImage.Name = "Jupiter",                               .RightImage.Name = "Man from Guatemala",                        .LeftImage.SampleOffset = 7515711,  .RightImage.SampleOffset = 7573015,  .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "U.N. Photo", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.043f, .DebounceSamples = 2}},
    {.Key = KEY_E,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Dancer from Bali",                          .LeftImage.SampleOffset = 8137606,  .RightImage.SampleOffset = 8086172,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "Donna Grosvenor"},
    {.Key = KEY_R,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Andean Girls",                              .LeftImage.SampleOffset = 8635865,  .RightImage.SampleOffset = 8592929,  .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "Joseph Scherschel"},
    {.Key = KEY_T,           .LeftImage.Name = "Earth",                                 .RightImage.Name = "Thailand Master Craftsman",                 .LeftImage.SampleOffset = 9142947,  .RightImage.SampleOffset = 9089631,  .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .RightImage.Source = "Dean Conger", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.05f, .DebounceSamples = 2}},
    {.Key = KEY_Y,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Elephant",                                  .LeftImage.SampleOffset = 9636814,  .RightImage.SampleOffset = 9600311,  .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.Source = "Peter Kunstadter"},
    {.Key = KEY_U,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Old Man Smoking",                           .LeftImage.SampleOffset = 10113065, .RightImage.SampleOffset = 10102506, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.Source = "Jonathon Blair", .RightImage.Description = "Turkey"},
    {.Key = KEY_I,           .LeftImage.Name = "Sinai Peninsula & the Nile",            .RightImage.Name = "Old Man with Dog & Flowers",                .LeftImage.SampleOffset = 10615931, .RightImage.SampleOffset = 10613180, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "NASA", .LeftImage.Description = "Low Earth Orbit; Annotated with the chemical composition of Earth's atmosphere.", .RightImage.Source = "Bruce Baumann"},
    {.Key = KEY_O,           .LeftImage.Name = "Chemical Definitions",                  .RightImage.Name = "Mountain Climber",                          .LeftImage.SampleOffset = 11130449, .RightImage.SampleOffset = 11125216, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Frank Drake", .RightImage.Source = "Gaston Rebuffat"},
    {.Key = KEY_P,           .LeftImage.Name = "DNA Structure",                         .RightImage.Name = "Gymnast",                                   .LeftImage.SampleOffset = 11611579, .RightImage.SampleOffset = 11631241, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Philip Leonian; Sports Illustrated", .RightImage.Description = "Cathy Rigby"},
    {.Key = KEY_A,           .LeftImage.Name = "DNA Structure (Magnified)",             .RightImage.Name = "Sprinters",                                 .LeftImage.SampleOffset = 12112408, .RightImage.SampleOffset = 12131300, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "History of the Olympics, Picturepoint, London", .RightImage.Description = "Valeriy Borzov of the U.S.S.R. in lead"},
    {.Key = KEY_S,           .LeftImage.Name = "Cell Division",                         .RightImage.Name = "Schoolroom",                                .LeftImage.SampleOffset = 12618348, .RightImage.SampleOffset = 12643425, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Turtox/Cambosco", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Japan"},
    {.Key = KEY_D,           .LeftImage.Name = "Anatomy 1",                             .RightImage.Name = "Children with Globe",                       .LeftImage.SampleOffset = 13155793, .RightImage.SampleOffset = 13158591, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Skeleton & Muscles, Front", .RightImage.Source = "U.N. Photo", .RightImage.Description = "U.N. International School"},
    {.Key = KEY_F,           .LeftImage.Name = "Anatomy 2",                             .RightImage.Name = "Cotton Harvest",                            .LeftImage.SampleOffset = 13667626, .RightImage.SampleOffset = 13664412, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Skeleton & Muscles, Back", .RightImage.Source = "Howell Walker"},
    {.Key = KEY_G,           .LeftImage.Name = "Anatomy 3",                             .RightImage.Name = "Grape Picker",                              .LeftImage.SampleOffset = 14169422, .RightImage.SampleOffset = 14179145, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Lungs & Kidneys, Back", .RightImage.Source = "David Moore"},
    {.Key = KEY_H,           .LeftImage.Name = "Anatomy 4",                             .RightImage.Name = "Supermarket",                               .LeftImage.SampleOffset = 14671718, .RightImage.SampleOffset = 14694679, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Lungs & Kidneys, Front", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Woman eating grapes"},
    {.Key = KEY_J,           .LeftImage.Name = "Anatomy 5",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 15186181, .RightImage.SampleOffset = 15192711, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Back",  .RightImage.Source = "Jerry Greenberg", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.027f, .DebounceSamples = 3}},
    {.Key = KEY_K,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 15679226, .RightImage.SampleOffset = 15692985, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Green, .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front", .RightImage.Source = "Jerry Greenberg"},
    {.Key = KEY_L,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Underwater Scene with Diver & Fish",        .LeftImage.SampleOffset = 16190230, .RightImage.SampleOffset = 16224544, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Blue,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front", .RightImage.Source = "Jerry Greenberg"},
    {.Key = KEY_Z,           .LeftImage.Name = "Anatomy 6",                             .RightImage.Name = "Fishing Boat with Nets",                    .LeftImage.SampleOffset = 16705655, .RightImage.SampleOffset = 16725175, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Internal Organs, Front", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Greece"},
    {.Key = KEY_X,           .LeftImage.Name = "Anatomy 7",                             .RightImage.Name = "Cooking Fish",                              .LeftImage.SampleOffset = 17189421, .RightImage.SampleOffset = 17242071, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Ribcage", .RightImage.Source = "Cooking of Spain and Portugal, Time-Life Books"},
    {.Key = KEY_C,           .LeftImage.Name = "Anatomy 8",                             .RightImage.Name = "Chinese Dinner Party",                      .LeftImage.SampleOffset = 17738425, .RightImage.SampleOffset = 17748573, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "World Book Encyclopedia", .LeftImage.Description = "Muscles", .RightImage.Source = "Time-Life Books", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.055f, .DebounceSamples = 4}},
    {.Key = KEY_V,           .LeftImage.Name = "Human Sex Organs",                      .RightImage.Name = "Licking, Eating and Drinking",              .LeftImage.SampleOffset = 18257464, .RightImage.SampleOffset = 18250532, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Sinauer Associates, Inc.", .LeftImage.Description = "Male & Female", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.032f, .DebounceSamples = 3}},
    {.Key = KEY_B,           .LeftImage.Name = "Conception (Diagram)",                  .RightImage.Name = "Great Wall of China",                       .LeftImage.SampleOffset = 18765554, .RightImage.SampleOffset = 18747087, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "H. Edward Kim"},
    {.Key = KEY_N,           .LeftImage.Name = "Conception",                            .RightImage.Name = "House Construction",                        .LeftImage.SampleOffset = 19277607, .RightImage.SampleOffset = 19244296, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Albert Bonniers; Forlag, Stockholm", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Cameroon"},
    {.Key = KEY_M,           .LeftImage.Name = "Fertilized Ovum",                       .RightImage.Name = "Construction Scene",                        .LeftImage.SampleOffset = 19788079, .RightImage.SampleOffset = 19745141, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Albert Bonniers; Forlag, Stockholm", .RightImage.Source = "William Albert Allard", .RightImage.Description = "Amish Country"},
    {.Key = KEY_COMMA,       .LeftImage.Name = "Fetus (Diagram)",                       .RightImage.Name = "House",                                     .LeftImage.SampleOffset = 20291384, .RightImage.SampleOffset = 20251104, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Ethiopia", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.032f, .DebounceSamples = 3}},
    {.Key = KEY_PERIOD,      .LeftImage.Name = "Fetus",                                 .RightImage.Name = "House",                                     .LeftImage.SampleOffset = 20837565, .RightImage.SampleOffset = 20753634, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Dr. Frank Allan", .RightImage.Source = "Robert Sisson", .RightImage.Description = "New England"},
    {.Key = KEY_SLASH,       .LeftImage.Name = "Diagram of Male and Female",            .RightImage.Name = "Modern House",                              .LeftImage.SampleOffset = 21336797, .RightImage.SampleOffset = 21271237, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Frank Drake", .RightImage.Description = "Cloudcroft, New Mexico"},
    {.Key = KEY_GRAVE,       .LeftImage.Name = "Birth",                                 .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 21854824, .RightImage.SampleOffset = 21770903, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Wayne Miller", .RightImage.Source = "Jim Amos"},
    {.ShiftKey = KEY_ONE,    .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 22366812, .RightImage.SampleOffset = 22271748, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Green, .LeftImage.Source = "U.N. Photo", .LeftImage.Description = "Philippines", .RightImage.Source = "Jim Amos"},
    {.ShiftKey = KEY_TWO,    .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "House Interior with Artist and Fire",       .LeftImage.SampleOffset = 22880833, .RightImage.SampleOffset = 22784699, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Blue,  .LeftImage.Source = "U.N. Photo", .LeftImage.Description = "Philippines", .RightImage.Source = "Jim Amos"},
    {.ShiftKey = KEY_THREE,  .LeftImage.Name = "Nursing Mother",                        .RightImage.Name = "Taj Mahal",                                 .LeftImage.SampleOffset = 23397216, .RightImage.SampleOffset = 23273941, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "U.N. Photo", .LeftImage.Description = "Philippines", .RightImage.Source = "David Carroll", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_FOUR,   .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "English City",                              .LeftImage.SampleOffset = 23914636, .RightImage.SampleOffset = 23793232, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey", .LeftImage.Description = "Malaysia", .RightImage.Source = "C.S. Lewis, Images of His World, William B. Eerdmans Publishing Co.", .RightImage.Description = "Oxford"},
    {.ShiftKey = KEY_FIVE,   .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "Boston",                                    .LeftImage.SampleOffset = 24433828, .RightImage.SampleOffset = 24280027, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey", .LeftImage.Description = "Malaysia", .RightImage.Source = "Ted Spiegel"},
    {.ShiftKey = KEY_SIX,    .LeftImage.Name = "Father and Daughter",                   .RightImage.Name = "U.N. Building (Day-time)",                  .LeftImage.SampleOffset = 24953317, .RightImage.SampleOffset = 24788532, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "David Harvey", .LeftImage.Description = "Malaysia", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City"},
    {.ShiftKey = KEY_SEVEN,  .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 25442468, .RightImage.SampleOffset = 25301194, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Red,   .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City"},
    {.ShiftKey = KEY_EIGHT,  .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 25948659, .RightImage.SampleOffset = 25815519, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Green, .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City"},
    {.ShiftKey = KEY_NULL,   .LeftImage.Name = "Group of Children",                     .RightImage.Name = "U.N. Building (Night-time)",                .LeftImage.SampleOffset = 26459594, .RightImage.SampleOffset = 26333242, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Ruby Mera, UNICEF", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Manhattan, New York City"},
    {.ShiftKey = KEY_NINE,   .LeftImage.Name = "Diagram of Family Ages",                .RightImage.Name = "Sydney Opera House",                        .LeftImage.SampleOffset = 26977596, .RightImage.SampleOffset = 26847380, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Mike Long"},
    {.ShiftKey = KEY_ZERO,   .LeftImage.Name = "Family Portrait",                       .RightImage.Name = "Artisan with Drill",                        .LeftImage.SampleOffset = 27497919, .RightImage.SampleOffset = 27377225, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Nina Leen, Time, Inc.", .RightImage.Source = "Frank Hewlett"},
    {.ShiftKey = KEY_Q,      .LeftImage.Name = "Diagram of Continental Drift",          .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 27990289, .RightImage.SampleOffset = 27885032, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Jon Lomberg", .LeftImage.Description = "Derived from LAGEOS plaque", .RightImage.Source = "Fred Ward"},
    {.ShiftKey = KEY_W,      .LeftImage.Name = "Structure of the Earth",                .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 28491246, .RightImage.SampleOffset = 28404984, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "Fred Ward", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.047f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_E,      .LeftImage.Name = "Heron Island",                          .RightImage.Name = "Factory Interior",                          .LeftImage.SampleOffset = 28984353, .RightImage.SampleOffset = 28930111, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "", .LeftImage.Description = "Great Barrier Reef of Australia", .RightImage.Source = "Fred Ward"},
    {.ShiftKey = KEY_R,      .LeftImage.Name = "Seashore",                              .RightImage.Name = "Museum",                                    .LeftImage.SampleOffset = 29484247, .RightImage.SampleOffset = 29431324, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Dick Smith; Cape Neddick, Maine", .RightImage.Source = "David Cupp"},
    {.ShiftKey = KEY_T,      .LeftImage.Name = "Snake River and Grand Tetons",          .RightImage.Name = "X-Ray of Hand",                             .LeftImage.SampleOffset = 29992082, .RightImage.SampleOffset = 29911490, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Ansel Adams", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.025f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_Y,      .LeftImage.Name = "Sand dunes",                            .RightImage.Name = "Woman with Microscope",                     .LeftImage.SampleOffset = 30490384, .RightImage.SampleOffset = 30419938, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "George Mobley", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Somalia", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.025f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_U,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Street Scene",                              .LeftImage.SampleOffset = 30991505, .RightImage.SampleOffset = 30930390, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Pakistan"},
    {.ShiftKey = KEY_I,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Rush Hour Traffic",                         .LeftImage.SampleOffset = 31501703, .RightImage.SampleOffset = 31439778, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "U.N. Photo", .RightImage.Description = "Thailand", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_O,      .LeftImage.Name = "Monument Valley",                       .RightImage.Name = "Modern Highway",                            .LeftImage.SampleOffset = 31986826, .RightImage.SampleOffset = 31939644, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Shostal Associates, Inc.", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Ithaca, New York"},
    {.ShiftKey = KEY_P,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Golden Gate Bridge",                        .LeftImage.SampleOffset = 32486942, .RightImage.SampleOffset = 32440627, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .RightImage.Source = "Ansel Adams"},
    {.ShiftKey = KEY_A,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Train",                                     .LeftImage.SampleOffset = 32978679, .RightImage.SampleOffset = 32945392, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .RightImage.Source = "Gordon Gahan", .RightImage.Description = "United Aircraft Corporation Turbotrain"},
    {.ShiftKey = KEY_S,      .LeftImage.Name = "Forest Scene with Mushrooms",           .RightImage.Name = "Airplane in Flight",                        .LeftImage.SampleOffset = 33489509, .RightImage.SampleOffset = 33469402, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Bruce Dale", .RightImage.Source = "Frank Drake", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.031f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_D,      .LeftImage.Name = "Leaf (Fragaria)",                       .RightImage.Name = "Airport",                                   .LeftImage.SampleOffset = 34005577, .RightImage.SampleOffset = 33984056, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Arthur Herrick", .RightImage.Source = "George Hunter", .RightImage.Description = "Toronto", .LeftImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.03f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_F,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Antarctic Expedition",                      .LeftImage.SampleOffset = 34523161, .RightImage.SampleOffset = 34490520, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .RightImage.Source = "National Geographic; Great Adventures with the National Geographic", .RightImage.Description = "Commonwealth Trans-Antarctic Expedition", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_G,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Radio Telescope",                           .LeftImage.SampleOffset = 35015433, .RightImage.SampleOffset = 34999599, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .RightImage.Source = "James Blair", .RightImage.Description = "Westerbork, Netherlands"},
    {.ShiftKey = KEY_H,      .LeftImage.Name = "Autumn Fallen Leaves",                  .RightImage.Name = "Radio Telescope",                           .LeftImage.SampleOffset = 35537491, .RightImage.SampleOffset = 35520804, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Jodi Cobb", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Arecibo"},
    {.ShiftKey = KEY_J,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Page of Book",                              .LeftImage.SampleOffset = 36022479, .RightImage.SampleOffset = 36038801, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "The System of the World. Page 6 of Issac Newton's Principia Mathematica (Volume III)"},
    {.ShiftKey = KEY_K,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 36547852, .RightImage.SampleOffset = 36582381, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Red,   .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .RightImage.Source = "NASA", .RightImage.Description = "Ed White"},
    {.ShiftKey = KEY_L,      .LeftImage.Name = "Snowflakes over Sequoia",               .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 37076695, .RightImage.SampleOffset = 37093950, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Josef Muench, Robert F. Sisson",        .RightImage.Source = "NASA", .RightImage.Description = "Ed White"},
    {.ShiftKey = KEY_Z,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Astronaut in Space",                        .LeftImage.SampleOffset = 37669499, .RightImage.SampleOffset = 37654503, .LeftImage.ColorChannel = Red,   .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .RightImage.Source = "NASA", .RightImage.Description = "Ed White"},
    {.ShiftKey = KEY_X,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Titan Centaur Launch",                      .LeftImage.SampleOffset = 38155490, .RightImage.SampleOffset = 38150712, .LeftImage.ColorChannel = Green, .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .RightImage.Source = "NASA"},
    {.ShiftKey = KEY_C,      .LeftImage.Name = "Tree with Daffodils",                   .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 38667959, .RightImage.SampleOffset = 38655546, .LeftImage.ColorChannel = Blue,  .RightImage.ColorChannel = Red,   .LeftImage.Source = "Gardens Winterthur, Winterthur Museum", .RightImage.Source = "David Harvey", .RightImage.OverrideParams = &(ScanTriggerThresholdParams){.FallThreshold = 0.04f, .DebounceSamples = 3}},
    {.ShiftKey = KEY_V,      .LeftImage.Name = "Flying Insect with Flowers",            .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 39157401, .RightImage.SampleOffset = 39177643, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Green, .LeftImage.Source = "Borne on the Wind, Stephen Dalton", .LeftImage.Description = "Ichneumonidae", .RightImage.Source = "David Harvey"},
    {.ShiftKey = KEY_B,      .LeftImage.Name = "Diagram of Vertebrate Evolution",       .RightImage.Name = "Sunset with Birds",                         .LeftImage.SampleOffset = 39651843, .RightImage.SampleOffset = 39671543, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Blue,  .LeftImage.Source = "Jon Lomberg", .RightImage.Source = "David Harvey"},
    {.ShiftKey = KEY_N,      .LeftImage.Name = "Seashell",                              .RightImage.Name = "String Quartet",                            .LeftImage.SampleOffset = 40149135, .RightImage.SampleOffset = 40171212, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Harry N. Abrams, Inc.", .LeftImage.Description = "Xancidae", .RightImage.Source = "Phillips Recordings", .RightImage.Description = "Quartetto Italiano"},
    {.ShiftKey = KEY_M,      .LeftImage.Name = "Dolphins",                              .RightImage.Name = "Violin with Music Score",                   .LeftImage.SampleOffset = 40702862, .RightImage.SampleOffset = 40670528, .LeftImage.ColorChannel = Mono,  .RightImage.ColorChannel = Mono,  .LeftImage.Source = "Thomas Nebbia", .RightImage.Source = "National Astronomy and Ionosphere Center, Cornell University (NAIC)", .RightImage.Description = "Cavatina"},
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

const char* GetImageNameFromSampleOffset(u32 SampleOffset, bool bLeftChannel)
{
    const char* Result = "";

    i32 ChannelIndex = GetChannelIndexFromSampleOffset(SampleOffset, bLeftChannel);
    if (ChannelIndex > -1)
    {
        if (bLeftChannel)
        {
            Result = ImageMappings[ChannelIndex].LeftImage.Name;
        }
        else
        {
            Result = ImageMappings[ChannelIndex].RightImage.Name;
        }
    }

    return Result;
}

EImageColorChannel GetColorChannelFromSampleOffset(u32 SampleOffset, bool bLeftChannel)
{
    EImageColorChannel Result = Mono;

    i32 ChannelIndex = GetChannelIndexFromSampleOffset(SampleOffset, bLeftChannel);
    if (ChannelIndex > -1)
    {
        if (bLeftChannel)
        {
            Result = ImageMappings[ChannelIndex].LeftImage.ColorChannel;
        }
        else
        {
            Result = ImageMappings[ChannelIndex].RightImage.ColorChannel;
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
        // f32 A = fabsf(Value);
        if (Value > Result) Result = Value;
    }

    return Result;
}

f32 SyncScore(f32* Samples, u64 TestIndex, u64 SampleWidth, u32 Channels, bool bLeft)
{
    f32 Result = 0;

    for (u64 k = 0; k < SampleWidth; k++)
    {
        f32 Value = Samples[(TestIndex+k) * Channels + (bLeft ? 0 : 1)];
        Result += fabsf(Value);

        // f32 Value = Samples[(TestIndex+k) * Channels + (bLeft ? 0 : 1)];
        // f32 A = fabsf(Value);
        // if (A > Result) Result = A;
    }

    return Result;
}

enum EdgeTrackingState
{
    IDLE     = 0,
    TRACKING = 1
};

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

    u32 State = IDLE;
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
        // f32 Diff     = fabsf(Next - Current);
        f32 Diff     = Next - Current;

        switch (State)
        {
            case IDLE:
            {
                if (Diff < 0 && fabsf(Diff) >= FallThreshold)
                {
                    TroughSampleIndex = i;
                    TroughSampleValue = Current;
                    StableCount = 0;
                    State = TRACKING;
                    // printf("diff: %f\nIndex: %d\nValue: %f\n", Diff, TroughSampleIndex, TroughSampleValue);
                }
            }
            break;

            case TRACKING:
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

                    State = IDLE;
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
    // TODO: enum
    u32 State = 0; // 0 = IDLE 1 = WAIT FOR OPPOSITE CROSSING
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
            case 0:
            {
                if (Current >= CrossingThreshold)
                {
                    State = 1;
                }
            }
            break;

            case 1:
            {
                if (Current <= -CrossingThreshold)
                {
                    NumCrossedZero++;
                    State = 0;
                }
            }
            break;
        }
    }
    
    return bResult;

}

bool DecodeImage_StepV2(f32* Samples, u64 StepIndex, Wave Wav, Image* OutputImage, Texture2D OutputTexture, bool bLeftChannel,
                      u32* Cursor, f32 Threshold, EImageColorChannel ColorChannel, ScanTriggerThresholdParams* OverrideParams)
{
    bool bSuccess = false;

    u32 SamplesPerLine = (f32)Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack          = 0.05 * (f32)SamplesPerLine;
    u32 NextLinePrediction = (*Cursor + (SamplesPerLine - Slack));

    i32 PeakIndex = -1;
    f32 PeakValue = 0;

    DetectScanTrigger(Samples, NextLinePrediction, Slack*2, Wav, bLeftChannel, OverrideParams, &PeakIndex, &PeakValue);

    // printf("%i | %f\n", NextLinePrediction + PeakIndex, PeakValue);

    f32 BestScore = SyncPeak(Samples, *Cursor, SamplesPerLine, Wav.channels, bLeftChannel) / SamplesPerLine;

    bool bIsBeep = DetectBeep(Samples, *Cursor, SamplesPerLine, Wav, bLeftChannel);

    if (bIsBeep)
    {
        printf("Beep!\n");
    }

    f32 Diff = fabsf(BestScore - Threshold);
    bool bWithinBand = Diff < 0.1f;
    // bool bWithinBand = Threshold / BestScore > 0.1f;
    // printf("Best: %f | Thresold: %f\n", BestScore, Threshold);
    if (bWithinBand && !bIsBeep)
    // if (BestScore >= Threshold)
    {
        u32 NewOffset = NextLinePrediction + PeakIndex;

        u32 NextLineSamplesActual = NewOffset - *Cursor;
        // printf("Samples: %u | Offset: %u\n", NextLineSamplesActual, NewOffset);
        // f64 Delta = NextLineSamplesActual - SamplesPerLine;
        // printf("line %llu: %f (delta: %f)\n", Line, NextLineSamplesActual, Delta);

        // f64 Black = -0.15;
        // f64 White = 0.1;
        f64 Black = -0.1;
        f64 White = 0.07;

        f64 ImageStart = *Cursor;
        f64 ImageLen   = NextLineSamplesActual;
        
        for (i32 y = 0; StepIndex < 600 && y < LineHeight; y++)
        {
            u64 SampleIndex0 = (u64)ImageStart + ((f64)y     / (f64)LineHeight) * ImageLen;
            u64 SampleIndex1 = (u64)ImageStart + ((f64)(y+1) / (f64)LineHeight) * ImageLen;
            if (SampleIndex1 <= SampleIndex0) { SampleIndex1 = SampleIndex0 + 1; }

            f64 Sum = 0;
            for (u64 i = SampleIndex0; i < SampleIndex1; i++)
            {
                Sum += Samples[i * Wav.channels + (bLeftChannel ? 0 : 1)];
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

            Color CurrentColor = GetImageColor(*OutputImage, StepIndex, y);
            Color PixelColor = {0};
            switch (ColorChannel)
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

            ImageDrawPixel(OutputImage, StepIndex, y, PixelColor);
        }

        *Cursor = NewOffset;
        bSuccess = true;
    }

    return bSuccess;
}

bool DecodeImage_Step(f32* Samples, u64 StepIndex, Wave Wav, Image* OutputImage, Texture2D OutputTexture, bool bLeftChannel,
                      u32* Cursor, u64 Threshold)
{
    bool bSuccess = false;

    f64 SyncBurstWidth = 10.0;
    f64 SamplesPerLine = (Wav.sampleRate * SAMPLES_FACTOR);// + SyncBurstWidth*5;
    f64 Slack = 0.1 * SamplesPerLine;

    f64 NextLinePrediction = *Cursor + SamplesPerLine;
    
    u64 SearchStart = (u64)(NextLinePrediction - Slack);
    u64 SearchEnd   = (u64)(NextLinePrediction + Slack);

    // now find the next peak (best score out of our search window)
    u64 BestIndex = 0;
    f64 BestScore = -1.0;
    for (u64 i = SearchStart; i < SearchEnd; i++)
    {
        f64 Score = SyncScore(Samples, i, SyncBurstWidth, Wav.channels, bLeftChannel);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = i;
        }
    }

    // find the burst's peak amplitude
    f32 Peak = 0.0f;
    for (u64 i = BestIndex; i < BestIndex + (u64)SyncBurstWidth; i++)
    {
        f32 A = fabsf(Samples[i * Wav.channels + (bLeftChannel ? 0 : 1)]);
        if (A > Peak) Peak = A;
    }

    // walk left from the coarse hit, then forward to the first 50%-of-peak crossing
    u64 Edge = BestIndex - (u64)SyncBurstWidth;
    while (fabsf(Samples[Edge * Wav.channels + (bLeftChannel ? 0 : 1)]) < Peak * 0.5f)
    {
        Edge++;
    }


    if (BestScore >= Threshold)
    {
        f64 NextLineSamplesActual = (f64)(BestIndex - *Cursor);
        f64 Delta = NextLineSamplesActual - SamplesPerLine;
        // printf("line %llu: %f (delta: %f)\n", Line, NextLineSamplesActual, Delta);

        f64 Black = -0.15;
        f64 White = 0.1;

        f64 ImageStart = *Cursor - SyncBurstWidth;
        f64 ImageLen   = NextLineSamplesActual;

        for (i32 y = 0; y < LineHeight; y++)
        {
            u64 SampleIndex0 = (u64)ImageStart + ((f64)y     / (f64)LineHeight) * ImageLen;
            u64 SampleIndex1 = (u64)ImageStart + ((f64)(y+1) / (f64)LineHeight) * ImageLen;
            if (SampleIndex1 <= SampleIndex0) { SampleIndex1 = SampleIndex0 + 1; }

            f64 Sum = 0;
            for (u64 i = SampleIndex0; i < SampleIndex1; i++)
            {
                Sum += Samples[i * Wav.channels + (bLeftChannel ? 0 : 1)];
            }
            f64 Avg = Sum/(f64)(SampleIndex1-SampleIndex0);

            i32 V = (i32)((Avg - Black) / (White - Black) * 255.0);
            if (V < 0) V = 0; if (V > 255) V = 255;

            V = 255 - V;

            Color PixelColor = (Color){ V, V, V, 255};
            ImageDrawPixel(OutputImage, StepIndex, y, PixelColor);
        }

        // Cursor = (f64)BestIndex;
        *Cursor = (f64)Edge;

        bSuccess = true;
    }
    else
    {
        bSuccess = false;
    }

    return bSuccess;
}

void DecodeImage(Wave Wav, u64 ImageSampleOffset, Image* OutputImage, Texture2D OutputTexture, bool bLeftChannel)
{
    f64 SyncBurstWidth = 10.0;
    f64 SamplesPerLine = 379.0;
    f64 Slack = 0.1 * SamplesPerLine;

    u64 StartFrame = ImageSampleOffset;

    // TODO: fix leak
    f32* Samples = LoadWaveSamples(Wav);

    f64 Score = SyncScore(Samples, StartFrame, SyncBurstWidth, Wav.channels, bLeftChannel);
    // printf("Burst Score: %f\n", Score);

    u32 Cursor = StartFrame;
    f64 Threshold = Score * 0.5;
    for (u64 Line = 0; Line < 2000; Line++)
    {
        if (DecodeImage_Step(Samples, Line, Wav, OutputImage, OutputTexture, bLeftChannel, &Cursor, Threshold))
        {
            // UpdateTexture(OutputTexture, OutputImage->data);
        }
        else
        {
            break;
        }
    }

    UpdateTexture(OutputTexture, OutputImage->data);
}


void JumpToMapping(RecordPlayer* Left, RecordPlayer* Right, u32 Index)
{
    ImageMapping M = ImageMappings[Index];

    Left->ImageOffset = M.LeftImage.SampleOffset;
    Left->Cursor = M.LeftImage.SampleOffset;
    Left->ScanLine = 0;

    Right->ImageOffset = M.RightImage.SampleOffset;
    Right->Cursor = M.RightImage.SampleOffset;
    Right->ScanLine = 0;
}

void SelectMapping(RecordPlayer* Left, RecordPlayer* Right, u32 Index,
                   f32* Samples, u32 SamplesPerLine, Wave Wav, Music GoldenWav)
{
    ImageMapping M = ImageMappings[Index];

    JumpToMapping(Left, Right, Index);

    Left->Threshold  = SyncPeak(Samples, Left->ImageOffset,  SamplesPerLine, Wav.channels, true);
    Right->Threshold = SyncPeak(Samples, Right->ImageOffset, SamplesPerLine, Wav.channels, false);

    u64 MusicPosition = M.LeftImage.SampleOffset < M.RightImage.SampleOffset ? M.LeftImage.SampleOffset : M.RightImage.SampleOffset;
    SeekMusicStream(GoldenWav, (f32)MusicPosition / (f32)Wav.sampleRate);
}

// long names like "Underwater scene with diver and fish" are wider than a column at the
// full title size, so step the size down until it fits rather than let it run into
// the neighbouring channel
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

        Reveal->Elapsed = bLaterColorPass ? RevealInstantSeconds : 0.0f;
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

void DrawChannelMetaData(Font TitleFont, Font BodyFont, ImageMetaData Data, TextReveal Reveal,
                         i32 ColumnX, i32 ColumnWidth, i32 NameY, i32 SourceY, i32 DescriptionY)
{
    i32 NameFontSize = FitFontSize(TitleFont, Data.Name, ColumnWidth, TitleFontSize, BodyFontSize);

    // bottom-align a shrunken title in its slot so the gap below stays constant
    i32 NameOffsetY = TitleFontSize - NameFontSize;

    i32 Budget           = (i32)(Reveal.Elapsed * RevealCharsPerSecond);
    i32 NameChars        = TakeRevealedChars(&Budget, Data.Name);
    i32 SourceChars      = TakeRevealedChars(&Budget, Data.Source);
    i32 DescriptionChars = TakeRevealedChars(&Budget, Data.Description);

    DrawRevealedText(TitleFont, Data.Name,        NameChars,        (Vector2){ColumnX, NameY + NameOffsetY}, NameFontSize, WHITE);
    DrawRevealedText(BodyFont,  Data.Source,      SourceChars,      (Vector2){ColumnX, SourceY},             BodyFontSize, LIGHTGRAY);
    DrawRevealedText(BodyFont,  Data.Description, DescriptionChars, (Vector2){ColumnX, DescriptionY},        BodyFontSize, GRAY);
}

// raylib key codes are ascii for everything the table binds, so the character is the code
const char* GetShortcutKeyLabel(ImageMapping Mapping)
{
    if (Mapping.ShiftKey != KEY_NULL) return TextFormat("S-%c", (char)Mapping.ShiftKey);
    if (Mapping.Key      != KEY_NULL) return TextFormat("  %c", (char)Mapping.Key);

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
    const char* FooterText = "TAP ESC TO CLOSE    HOLD ESC TO QUIT    C-F FULLSCREEN    LEFT/RIGHT STEP IMAGES";
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

void DrawChannelWaveform(f32* Samples, Wave Wav, f32 MusicCursor,
                         u32 SamplesPerLine, u32 NumSamplesToDraw, bool bLeftChannel,
                         i32 BaseLocationX, i32 BaseLocationY, i32 ScanWidth, i32 ScanHeight)
{
    if (MusicCursor <= SamplesPerLine * NumSamplesToDraw)
    {
        return;
    }

    u32 Channel = bLeftChannel ? 0 : 1;

    u32 Draw_StartPointX = BaseLocationX;
    u32 Draw_EndPointX   = BaseLocationX + (ScanWidth  * ImageScale - Scaled(140));
    u32 Draw_MidPointY   = BaseLocationY + (ScanHeight * ImageScale + Scaled(65));

    u32 Window      = SamplesPerLine * NumSamplesToDraw;
    u32 CursorStart = MusicCursor - Window;
    u32 CursorEnd   = MusicCursor;

    u32 Slack = 0.05 * SamplesPerLine;
    i32 TriggerRel = 0;
    DetectScanTrigger(Samples, CursorStart, Slack, Wav, bLeftChannel, NULL, &TriggerRel, NULL);

    if (TriggerRel > 0)
    {
        CursorStart = CursorStart + TriggerRel;
        Window      = CursorEnd - CursorStart;
    }

    for (u32 i = CursorStart; i < CursorEnd; i++)
    {
        f32 Value = Samples[i * Wav.channels + Channel] * 350.0f * UIScale;

        f32 Alpha = ((f32)(CursorEnd - i) / Window);

        u32 XOffset = Draw_StartPointX + (i32)((Draw_EndPointX - Draw_StartPointX) * (1.0f - Alpha));

        // one sample per design pixel, so the mark widens with the scale or the trace goes dotted
        DrawRectangle(XOffset, Draw_MidPointY + Value, Scaled(1), Scaled(1), WHITE);
    }
}

// returns true when the baked font sizes moved, which is the caller's cue to rebake them
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

    // dragging a window edge moves the scale every frame, but rasterising is only worth it
    // when a size actually lands on a different integer
    return TitleFontSize     != PreviousTitleFontSize
        || BodyFontSize      != PreviousBodyFontSize
        || SmallFontSize     != PreviousSmallFontSize
        || MenuFontSize      != PreviousMenuFontSize
        || MenuTitleFontSize != PreviousMenuTitleFontSize;
}

// glyphs bake at a fixed pixel size, so the whole set gets rebuilt whenever the scale moves
void LoadUIFonts(UIFonts* Fonts)
{
    // a retina framebuffer is denser than the coordinate space drawn into, so glyphs bake at the
    // pixels they will really cover and get drawn back down at the size the layout asked for.
    // one texel per pixel either way, which is what keeps the text sharp
    f32 PixelDensity = GetWindowScaleDPI().x;
    if (PixelDensity < 1.0f)
    {
        PixelDensity = 1.0f;
    }

    Fonts->Title = LoadFontEx("resources/IBMPlexMono-Bold.ttf",    (i32)(TitleFontSize    * PixelDensity), NULL, 0);
    Fonts->Body  = LoadFontEx("resources/IBMPlexMono-Regular.ttf", (i32)(BodyFontSize     * PixelDensity), NULL, 0);
    Fonts->Small = LoadFontEx("resources/IBMPlexMono-Regular.ttf", (i32)(SmallFontSize    * PixelDensity), NULL, 0);
    Fonts->Menu  = LoadFontEx("resources/IBMPlexMono-Regular.ttf", (i32)(MenuFontSize      * PixelDensity), NULL, 0);

    // the big title is the one face drawn well below the size it bakes at, in the menu header
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

i32 main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(DesignWidth, DesignHeight, "Golden Decoder");
    SetWindowMinSize(MinWindowWidth, MinWindowHeight);
    FitWindowToMonitor();
    SetTargetFPS(0);

    // escape opens the menu instead of closing the window out from under us
    SetExitKey(KEY_NULL);

    InitAudioDevice();

    Music GoldenWav = LoadMusicStream("resources/golden.wav");
    Wave Wav = LoadWave("resources/golden.wav");

    bool bHaveWave  = IsWaveValid(Wav);
    bool bHaveMusic = IsMusicValid(GoldenWav);

    f32* Samples = bHaveWave ? LoadWaveSamples(Wav) : NULL;

    if (!bHaveMusic || !bHaveWave || Samples == NULL)
    {
        TraceLog(LOG_ERROR, "resources/golden.wav is missing or is not a readable wav file.");
        TraceLog(LOG_ERROR, "if this is a fresh clone, the audio is stored in git lfs: install it and run \"git lfs pull\".");

        UnloadWaveSamples(Samples);
        UnloadWave(Wav);
        UnloadMusicStream(GoldenWav);
        CloseAudioDevice();
        CloseWindow();

        return 1;
    }

    PlayMusicStream(GoldenWav);

    Image Scan_Left = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Left = LoadTextureFromImage(Scan_Left);
    
    Image Scan_Right = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Right = LoadTextureFromImage(Scan_Right);

    // turn this off to see the raw pixels without any filtering
    SetTextureFilter(ScanTexture_Right, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ScanTexture_Left, TEXTURE_FILTER_BILINEAR);

    UpdateLayoutScale();

    UIFonts Fonts = {0};
    LoadUIFonts(&Fonts);

    u32 SamplesPerLine = Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack = 0.05 * SamplesPerLine;

    RecordPlayer Player_LeftChannel = {.ScanImage = &Scan_Left, .ScanTexture = ScanTexture_Left};
    RecordPlayer Player_RightChannel = {.ScanImage = &Scan_Right, .ScanTexture = ScanTexture_Right};

    Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, Slack, Wav.channels, true) / Slack;
    Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, Slack, Wav.channels, false) / Slack;

    bool bPaused = false;

    TextReveal Reveal_LeftChannel  = {.MappingIndex = -1};
    TextReveal Reveal_RightChannel = {.MappingIndex = -1};

    bool bMenuOpen        = false;
    bool bMenuEscapeArmed = false;
    bool bQuitRequested   = false;
    f32  MenuEscapeHeld   = 0.0f;

    while (!WindowShouldClose() && !bQuitRequested)
    {
        UpdateMusicStream(GoldenWav);

        if (UpdateLayoutScale())
        {
            UnloadUIFonts(&Fonts);
            LoadUIFonts(&Fonts);
        }

        const i32 LeftPadding = Scaled(0);
        const i32 ColumnWidth = LeftOffset - LeftPadding*2;

        i32 BaseLocationX = GetScreenWidth()/2  - LeftOffset;
        i32 BaseLocationY = GetScreenHeight()/2 - Scaled(375);

        const i32 DescriptionY = BaseLocationY  - BodyFontSize  - Scaled(9);
        const i32 SourceY      = DescriptionY   - BodyFontSize  - Scaled(2);
        const i32 NameY        = SourceY        - TitleFontSize - Scaled(4);
        const i32 CursorY      = NameY          - SmallFontSize - Scaled(4);

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

        bool bIsControl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (bIsControl && IsKeyPressed(KEY_F))
        {
            ToggleBorderlessWindowed();
        }

        if (IsKeyPressed(KEY_SPACE))
        {
            bPaused = !bPaused;
            if (bPaused)
            {
                PauseMusicStream(GoldenWav);
            }
            else
            {
                ResumeMusicStream(GoldenWav);
            }
        }

        u32 NumMappings = sizeof(ImageMappings) / sizeof(ImageMappings[0]);

        if (!bIsControl)
        {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))
            {
                i32 Step = IsKeyPressed(KEY_RIGHT) ? 1 : -1;
    
                i32 Target = GetChannelIndexFromSampleOffset(Player_LeftChannel.Cursor, true) + Step;
    
                if (Target >= 0 && Target < (i32)NumMappings)
                {
                    SelectMapping(&Player_LeftChannel, &Player_RightChannel, Target, Samples, SamplesPerLine, Wav, GoldenWav);
    
                    bMenuOpen = false;
                }
            }
        }

        // ctrl is a window modifier, so it never picks an image out from under the shortcut
        if (!bIsControl)
        {
            for (u32 i = 0; i < NumMappings; i++)
            {
                ImageMapping M = ImageMappings[i];
                bool bIsShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if (bIsShift)
                {
                    if (IsKeyPressed(M.ShiftKey))
                    {
                        SelectMapping(&Player_LeftChannel, &Player_RightChannel, i, Samples, SamplesPerLine, Wav, GoldenWav);

                        // picking a shortcut gets you out of the way of the image you picked
                        bMenuOpen = false;

                        break;
                    }
                }
                else
                {
                    if (IsKeyPressed(M.Key))
                    {
                        SelectMapping(&Player_LeftChannel, &Player_RightChannel, i, Samples, SamplesPerLine, Wav, GoldenWav);

                        // picking a shortcut gets you out of the way of the image you picked
                        bMenuOpen = false;

                        break;
                    }
                }
            }
        }

        f32 MusicCursor = (f32)GetMusicTimePlayed(GoldenWav) * (f32)Wav.sampleRate;

        {
            f32 Peak = SyncPeak(Samples, Player_LeftChannel.Cursor, SamplesPerLine, Wav.channels, true);
            Player_LeftChannel.Threshold = Peak/SamplesPerLine;
        }

        while (Player_LeftChannel.Cursor + SamplesPerLine <= MusicCursor)
        {
            // this loop can cross an image boundary, so every line decodes with its own image's tuning
            ImageMetaData LineData = GetImageMetaDataFromSampleOffset(Player_LeftChannel.Cursor, true);

            if (DecodeImage_StepV2(Samples, Player_LeftChannel.ScanLine, Wav, Player_LeftChannel.ScanImage, Player_LeftChannel.ScanTexture, true, &Player_LeftChannel.Cursor, Player_LeftChannel.Threshold, LineData.ColorChannel, LineData.OverrideParams))
            {
                Player_LeftChannel.ScanLine++;
            }
            else
            {
                Player_LeftChannel.ScanLine = 0;
                Player_LeftChannel.Cursor = MusicCursor;
                break;
            }
        }

        {

            f32 Peak = SyncPeak(Samples, Player_RightChannel.Cursor, SamplesPerLine, Wav.channels, false);
            Player_RightChannel.Threshold = Peak/SamplesPerLine;
        }

        while (Player_RightChannel.Cursor + SamplesPerLine <= MusicCursor)
        {
            ImageMetaData LineData = GetImageMetaDataFromSampleOffset(Player_RightChannel.Cursor, false);

            if (DecodeImage_StepV2(Samples, Player_RightChannel.ScanLine, Wav, Player_RightChannel.ScanImage, Player_RightChannel.ScanTexture, false, &Player_RightChannel.Cursor, Player_RightChannel.Threshold, LineData.ColorChannel, LineData.OverrideParams))
            {
                Player_RightChannel.ScanLine++;
            }
            else
            {
                Player_RightChannel.ScanLine = 0;
                Player_RightChannel.Cursor = MusicCursor;
                break;
            }
        }

        UpdateTexture(Player_RightChannel.ScanTexture, Player_RightChannel.ScanImage->data);
        UpdateTexture(Player_LeftChannel.ScanTexture, Player_LeftChannel.ScanImage->data);

        // the decode loops moved the cursors, so this is what the cursors are actually sitting on now
        ImageMetaData LeftData  = GetImageMetaDataFromSampleOffset(Player_LeftChannel.Cursor, true);
        ImageMetaData RightData = GetImageMetaDataFromSampleOffset(Player_RightChannel.Cursor, false);

        BeginDrawing();

        ClearBackground(BLACK);

        // how far along the record we are, spanning the top edge of the window
        {
            const i32 BarHeight = Scaled(4);

            f32 TrackLength = GetMusicTimeLength(GoldenWav);
            f32 Progress    = TrackLength > 0.0f ? GetMusicTimePlayed(GoldenWav) / TrackLength : 0.0f;

            if (Progress > 1.0f)
            {
                Progress = 1.0f;
            }

            DrawRectangle(0, 0, GetScreenWidth(), BarHeight, Fade(WHITE, 0.15f));
            DrawRectangle(0, 0, (i32)(GetScreenWidth() * Progress), BarHeight, WHITE);
        }

        DrawTextureEx(ScanTexture_Left, (Vector2){BaseLocationX, BaseLocationY}, 0, ImageScale, WHITE);
        DrawTextureEx(ScanTexture_Right, (Vector2){BaseLocationX+LeftOffset, BaseLocationY}, 0, ImageScale, WHITE);

        // timer and transport state
        {
            f32 PlayedTime = GetMusicTimePlayed(GoldenWav);
            const char* TimeText = TextFormat("%02i:%02i:%03i", (i32)PlayedTime / 60, (i32)PlayedTime % 60, (i32)(PlayedTime * 1000) % 1000);

            i32 GlyphSize = Scaled(14);
            i32 GlyphGap  = Scaled(12);

            i32 GroupWidth = GlyphSize + GlyphGap + (i32)MeasureTextEx(Fonts.Body, TimeText, BodyFontSize, FontSpacing).x;
            i32 GroupX     = GetScreenWidth() - GroupWidth;
            i32 GroupY     = BaseLocationY - Scaled(160);

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

        i32 LeftColumnX  = BaseLocationX + LeftPadding;
        i32 RightColumnX = BaseLocationX + LeftOffset + LeftPadding;

        // the reveal is part of the presentation, so it holds still with everything else
        f32 RevealDelta = bPaused ? 0.0f : GetFrameTime();

        UpdateTextReveal(&Reveal_LeftChannel,  GetChannelIndexFromSampleOffset(Player_LeftChannel.Cursor, true),
                         LeftData.ColorChannel,  RevealDelta);

        UpdateTextReveal(&Reveal_RightChannel, GetChannelIndexFromSampleOffset(Player_RightChannel.Cursor, false),
                         RightData.ColorChannel, RevealDelta);

        DrawChannelMetaData(Fonts.Title, Fonts.Body, LeftData,  Reveal_LeftChannel,
                            LeftColumnX,  ColumnWidth, NameY, SourceY, DescriptionY);

        DrawChannelMetaData(Fonts.Title, Fonts.Body, RightData, Reveal_RightChannel,
                            RightColumnX, ColumnWidth, NameY, SourceY, DescriptionY);

        // current decode cursor for each channel
        {
            DrawTextEx(Fonts.Small, TextFormat("L %u", Player_LeftChannel.Cursor),  (Vector2){LeftColumnX,  CursorY}, SmallFontSize, FontSpacing, GRAY);
            DrawTextEx(Fonts.Small, TextFormat("R %u", Player_RightChannel.Cursor), (Vector2){RightColumnX, CursorY}, SmallFontSize, FontSpacing, GRAY);
        }

        static u32 NumSamplesToDraw = 6;
        if (!bIsControl && IsKeyPressed(KEY_UP))
        {
            NumSamplesToDraw++;
            // printf("%u\n", NumSamplesToDraw);
        }
        if (!bIsControl && IsKeyPressed(KEY_DOWN))
        {
            NumSamplesToDraw--;
            // printf("%u\n", NumSamplesToDraw);
        }

        DrawChannelWaveform(Samples, Wav, Player_LeftChannel.Cursor, SamplesPerLine, NumSamplesToDraw,
                            true,  BaseLocationX,       BaseLocationY, Scan_Left.width, Scan_Left.height);

        DrawChannelWaveform(Samples, Wav, Player_RightChannel.Cursor, SamplesPerLine, NumSamplesToDraw,
                            false, BaseLocationX + LeftOffset, BaseLocationY, Scan_Left.width, Scan_Left.height);

        if (bMenuOpen)
        {
            DrawShortcutMenu(Fonts.Title, Fonts.Menu, MenuEscapeHeld,
                             GetChannelIndexFromSampleOffset(Player_LeftChannel.Cursor, true));
        }

        EndDrawing();
    }

    UnloadUIFonts(&Fonts);

    UnloadTexture(ScanTexture_Left);
    UnloadTexture(ScanTexture_Right);
    UnloadImage(Scan_Left);
    UnloadImage(Scan_Right);
    UnloadWaveSamples(Samples);

    CloseWindow();

    return 0;
}
