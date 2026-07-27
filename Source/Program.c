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

#define ClampMax(a, b) (a) > (b) ? (a) : (b)
#define ClampMin(a, b) (a) < (b) ? (a) : (b)

typedef u8 EImageColorChannel;
enum
{
    Mono  = 0,
    Red   = 1,
    Green = 2,
    Blue  = 3
};

typedef struct
{
    KeyboardKey Key;
    KeyboardKey ShiftKey;
    const char* LeftImageName;
    const char* LeftImagePhotographer;
    const char* RightImageName;
    const char* RightImagePhotographer;
    u32 LeftChannelOffset;
    u32 RightChannelOffset;
    EImageColorChannel LeftColorChannel;
    EImageColorChannel RightColorChannel;
} ImageMapping;


typedef struct
{
    u32 ImageOffset;
    f32 Threshold;
    u32 Cursor;
    u32 ScanLine;
    Image* ScanImage;
    Texture2D ScanTexture;
} RecordPlayer;


#define OFFSET_SCALING 2
#define SAMPLES_FACTOR (1.0f/(120.0f/(f32)(OFFSET_SCALING)))

// TODO: make these clickable
// TODO: slow mo scanline slider, so you can see the vertical pixel lines filling in realtime
// TODO: space bar to pause/play
// TODO: show photographer name and description for each image.
// TODO: clickable web links to go to original source and an auxiliary links
// TODO: quote at beginning of the experience. tap any key to skip to the decoding
// TODO: optional to play "the sounds of earth" instead of the noise
// TODO: google maps coordinates of where pictures were taken
// TODO: export to jpeg

// names referenced from: 
//    https://science.nasa.gov/mission/voyager/golden-record-contents/images/
//    https://en.wikipedia.org/wiki/Contents_of_the_Voyager_Golden_Record

// yes i really did type this by hand
ImageMapping ImageMappings[] =
{
    {.Key = KEY_ONE,         .LeftImageName = "Calibration Circle",                    .RightImageName = "School of fish",               .LeftChannelOffset = 1375283,  .RightChannelOffset = 1490087,  .LeftColorChannel = Mono,  .RightColorChannel = Red  },
    {.Key = KEY_TWO,         .LeftImageName = "Milky Way Location",                    .RightImageName = "School of fish",               .LeftChannelOffset = 1905722,  .RightChannelOffset = 1984822,  .LeftColorChannel = Mono,  .RightColorChannel = Green},
    {.Key = KEY_THREE,       .LeftImageName = "Math Definitions",                      .RightImageName = "School of fish",               .LeftChannelOffset = 2446590,  .RightChannelOffset = 2505754,  .LeftColorChannel = Mono,  .RightColorChannel = Blue },
    {.Key = KEY_FOUR,        .LeftImageName = "Physics Definitions",                   .RightImageName = "Tree Toad",                    .LeftChannelOffset = 2968481,  .RightChannelOffset = 3033022,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_FIVE,        .LeftImageName = "Solar System Parameters 1",             .RightImageName = "Crocodile",                    .LeftChannelOffset = 3483550,  .RightChannelOffset = 3546161,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_SIX,         .LeftImageName = "Solar System Parameters 2",             .RightImageName = "Eagle",                        .LeftChannelOffset = 3998630,  .RightChannelOffset = 4051738,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_SEVEN,       .LeftImageName = "The Sun (Hale Observations)",           .RightImageName = "Zebras",                       .LeftChannelOffset = 4500698,  .RightChannelOffset = 4544028,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_EIGHT,       .LeftImageName = "Solar Spectrum",                        .RightImageName = "Jane Goodall & Chimps",        .LeftChannelOffset = 4984064,  .RightChannelOffset = 5036615,  .LeftColorChannel = Red,   .RightColorChannel = Red},
    {.Key = KEY_NINE,        .LeftImageName = "Solar Spectrum",                        .RightImageName = "Jane Goodall & Chimps",        .LeftChannelOffset = 5467791,  .RightChannelOffset = 5543004,  .LeftColorChannel = Green, .RightColorChannel = Green},
    {.Key = KEY_NULL,        .LeftImageName = "Solar Spectrum",                        .RightImageName = "Jane Goodall & Chimps",        .LeftChannelOffset = 5955425,  .RightChannelOffset = 6039902,  .LeftColorChannel = Blue,  .RightColorChannel = Blue},
    {.Key = KEY_ZERO,        .LeftImageName = "Mercury",                               .RightImageName = "Sketch of Bushmen Hunters",    .LeftChannelOffset = 6582824,  .RightChannelOffset = 6570356,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_Q,           .LeftImageName = "Mars",                                  .RightImageName = "Bushmen Hunters",              .LeftChannelOffset = 7091405,  .RightChannelOffset = 7067636,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_W,           .LeftImageName = "Jupiter",                               .RightImageName = "Man from Guatemala",           .LeftChannelOffset = 7515711,  .RightChannelOffset = 7573015,  .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_E,           .LeftImageName = "Earth",                                 .RightImageName = "Dancer from Bali",             .LeftChannelOffset = 8137606,  .RightChannelOffset = 8086172,  .LeftColorChannel = Red,   .RightColorChannel = Mono},
    {.Key = KEY_R,           .LeftImageName = "Earth",                                 .RightImageName = "Andean Girls",                 .LeftChannelOffset = 8635865,  .RightChannelOffset = 8592929,  .LeftColorChannel = Green, .RightColorChannel = Mono},
    {.Key = KEY_T,           .LeftImageName = "Earth",                                 .RightImageName = "Thailand Master Craftsman",    .LeftChannelOffset = 9142947,  .RightChannelOffset = 9089631,  .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.Key = KEY_Y,           .LeftImageName = "Sinai Peninsula",                       .RightImageName = "Elephant",                     .LeftChannelOffset = 9636814,  .RightChannelOffset = 9600311,  .LeftColorChannel = Red,   .RightColorChannel = Mono},
    {.Key = KEY_U,           .LeftImageName = "Sinai Peninsula",                       .RightImageName = "Old Man Smoking Cigarette",    .LeftChannelOffset = 10113065, .RightChannelOffset = 10102506, .LeftColorChannel = Green, .RightColorChannel = Mono},
    {.Key = KEY_I,           .LeftImageName = "Sinai Peninsula",                       .RightImageName = "Old Man with Dog and Flowers", .LeftChannelOffset = 10615931, .RightChannelOffset = 10613180, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.Key = KEY_O,           .LeftImageName = "Chemical Definitions",                  .RightImageName = "Mountain climber",             .LeftChannelOffset = 11130449, .RightChannelOffset = 11125216, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_P,           .LeftImageName = "DNA Structure",                         .RightImageName = "Gymnast (Cathy Rigby)",        .LeftChannelOffset = 11611579, .RightChannelOffset = 11631241, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_A,           .LeftImageName = "DNA Structure (Magnified)",             .RightImageName = "Sprinters (Valeriy Borzov of the U.S.S.R. in lead)", .LeftChannelOffset = 12112408, .RightChannelOffset = 12131300, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_S,           .LeftImageName = "Cell Division",                         .RightImageName = "Schoolroom (Japan)",           .LeftChannelOffset = 12618348, .RightChannelOffset = 12643425, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_D,           .LeftImageName = "Anatomy 1 (Skeleton & Muscles, Front)", .RightImageName = "Children with globe (U.N. Intl. School)", .LeftChannelOffset = 13155793, .RightChannelOffset = 13158591, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_F,           .LeftImageName = "Anatomy 2 (Skeleton & Muscles, Back)",  .RightImageName = "Cotton harvest",               .LeftChannelOffset = 13667626, .RightChannelOffset = 13664412, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_G,           .LeftImageName = "Anatomy 3 (Lungs & Kidneys, Back)",     .RightImageName = "Grape picker",                 .LeftChannelOffset = 14169422, .RightChannelOffset = 14179145, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_H,           .LeftImageName = "Anatomy 4 (Lungs & Kidneys, Front)",    .RightImageName = "Supermarket",                  .LeftChannelOffset = 14671718, .RightChannelOffset = 14694679, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_J,           .LeftImageName = "Anatomy 5 (Internal Organs, Back)",     .RightImageName = "Underwater scene with diver and fish", .LeftChannelOffset = 15186181, .RightChannelOffset = 15192711, .LeftColorChannel = Mono,  .RightColorChannel = Red},
    {.Key = KEY_K,           .LeftImageName = "Anatomy 6 (Internal Organs, Front)",    .RightImageName = "Underwater scene with diver and fish", .LeftChannelOffset = 15679226, .RightChannelOffset = 15692985, .LeftColorChannel = Red,   .RightColorChannel = Green},
    {.Key = KEY_L,           .LeftImageName = "Anatomy 6 (Internal Organs, Front)",    .RightImageName = "Underwater scene with diver and fish", .LeftChannelOffset = 16190230, .RightChannelOffset = 16224544, .LeftColorChannel = Green, .RightColorChannel = Blue},
    {.Key = KEY_Z,           .LeftImageName = "Anatomy 6 (Internal Organs, Front)",    .RightImageName = "Fishing boat with nets (Greece)", .LeftChannelOffset = 16705655, .RightChannelOffset = 16725175, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.Key = KEY_X,           .LeftImageName = "Anatomy 7 (Ribcage)",                   .RightImageName = "Cooking Fish",                 .LeftChannelOffset = 17189421, .RightChannelOffset = 17242071, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_C,           .LeftImageName = "Anatomy 8 (Muscles)",                   .RightImageName = "Chinese Dinner Party",         .LeftChannelOffset = 17738425, .RightChannelOffset = 17748573, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_V,           .LeftImageName = "Human Sex Organs (Male & Female)",      .RightImageName = "Licking, eating and drinking", .LeftChannelOffset = 18257464, .RightChannelOffset = 18250532, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_B,           .LeftImageName = "Conception (Diagram)",                  .RightImageName = "Great Wall of China",          .LeftChannelOffset = 18765554, .RightChannelOffset = 18747087, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_N,           .LeftImageName = "Conception",                            .RightImageName = "House construction (Cameroon)",.LeftChannelOffset = 19277607, .RightChannelOffset = 19244296, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_M,           .LeftImageName = "Fertilized Ovum",                       .RightImageName = "Construction scene (Amish country)", .LeftChannelOffset = 19788079, .RightChannelOffset = 19745141, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_COMMA,       .LeftImageName = "Fetus (Diagram)",                       .RightImageName = "House (Ethiopia)",             .LeftChannelOffset = 20291384, .RightChannelOffset = 20251104, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_PERIOD,      .LeftImageName = "Fetus",                                 .RightImageName = "House (New England)",          .LeftChannelOffset = 20837565, .RightChannelOffset = 20753634, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_SLASH,       .LeftImageName = "Diagram of male and female",            .RightImageName = "Modern House (Cloudcroft, New Mexico)", .LeftChannelOffset = 21336797, .RightChannelOffset = 21271237, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.Key = KEY_GRAVE,       .LeftImageName = "Birth",                                 .RightImageName = "House interior with artist and fire",   .LeftChannelOffset = 21854824, .RightChannelOffset = 21770903, .LeftColorChannel = Mono,  .RightColorChannel = Red},
    {.ShiftKey = KEY_ONE,    .LeftImageName = "Nursing mother (Philippines)",          .RightImageName = "House interior with artist and fire",   .LeftChannelOffset = 22366812, .RightChannelOffset = 22271748, .LeftColorChannel = Red,   .RightColorChannel = Green},
    {.ShiftKey = KEY_TWO,    .LeftImageName = "Nursing mother (Philippines)",          .RightImageName = "House interior with artist and fire",   .LeftChannelOffset = 22880833, .RightChannelOffset = 22784699, .LeftColorChannel = Green, .RightColorChannel = Blue},
    {.ShiftKey = KEY_THREE,  .LeftImageName = "Nursing mother (Philippines)",          .RightImageName = "Taj Mahal",                             .LeftChannelOffset = 23397216, .RightChannelOffset = 23273941, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_FOUR,   .LeftImageName = "Father and daughter (Malaysia)",        .RightImageName = "English city (Oxford)",                 .LeftChannelOffset = 23914636, .RightChannelOffset = 23793232, .LeftColorChannel = Red,   .RightColorChannel = Mono},
    {.ShiftKey = KEY_FIVE,   .LeftImageName = "Father and daughter (Malaysia)",        .RightImageName = "Boston",                                .LeftChannelOffset = 24433828, .RightChannelOffset = 24280027, .LeftColorChannel = Green, .RightColorChannel = Mono},
    {.ShiftKey = KEY_SIX,    .LeftImageName = "Father and daughter (Malaysia)",        .RightImageName = "UN Building (Day)",                       .LeftChannelOffset = 24953317, .RightChannelOffset = 24788532, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_SEVEN,  .LeftImageName = "Group of children",                     .RightImageName = "UN Building (Night)",                   .LeftChannelOffset = 25442468, .RightChannelOffset = 25301194, .LeftColorChannel = Red,   .RightColorChannel = Red},
    {.ShiftKey = KEY_EIGHT,  .LeftImageName = "Group of children",                     .RightImageName = "UN Building (Night)",                   .LeftChannelOffset = 25948659, .RightChannelOffset = 25815519, .LeftColorChannel = Green, .RightColorChannel = Green},
    {.ShiftKey = KEY_NULL,   .LeftImageName = "Group of children",                     .RightImageName = "UN Building (Night)",                   .LeftChannelOffset = 26459594, .RightChannelOffset = 26333242, .LeftColorChannel = Blue,  .RightColorChannel = Blue},
    {.ShiftKey = KEY_NINE,   .LeftImageName = "Diagram of family ages",                .RightImageName = "Sydney Opera House",                    .LeftChannelOffset = 26977596, .RightChannelOffset = 26847380, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_ZERO,   .LeftImageName = "Family portrait",                       .RightImageName = "Artisan with drill",                    .LeftChannelOffset = 27497919, .RightChannelOffset = 27377225, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_Q,      .LeftImageName = "Diagram of continental drift (derived from LAGEOS plaque)", .RightImageName = "Factory interior",  .LeftChannelOffset = 27990289, .RightChannelOffset = 27885032, .LeftColorChannel = Mono,  .RightColorChannel = Red},
    {.ShiftKey = KEY_W,      .LeftImageName = "Structure of the Earth",                .RightImageName = "Factory interior",                      .LeftChannelOffset = 28491246, .RightChannelOffset = 28404984, .LeftColorChannel = Mono,  .RightColorChannel = Green},
    {.ShiftKey = KEY_E,      .LeftImageName = "Heron Island (Great Barrier Reef of Australia)", .RightImageName = "Factory interior",             .LeftChannelOffset = 28984353, .RightChannelOffset = 28930111, .LeftColorChannel = Mono,  .RightColorChannel = Blue},
    {.ShiftKey = KEY_R,      .LeftImageName = "Seashore (Cape Neddick, Maine)",        .RightImageName = "Museum",                                .LeftChannelOffset = 29484247, .RightChannelOffset = 29431324, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_T,      .LeftImageName = "Snake River and Grand Tetons",          .RightImageName = "X-ray of hand",                         .LeftChannelOffset = 29992082, .RightChannelOffset = 29911490, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_Y,      .LeftImageName = "Sand dunes",                            .RightImageName = "Woman with microscope (Somalia)",       .LeftChannelOffset = 30490384, .RightChannelOffset = 30419938, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_U,      .LeftImageName = "Monument Valley",                       .RightImageName = "Street scene (Pakistan)",               .LeftChannelOffset = 30991505, .RightChannelOffset = 30930390, .LeftColorChannel = Red,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_I,      .LeftImageName = "Monument Valley",                       .RightImageName = "Rush hour traffic (Thailand)[",         .LeftChannelOffset = 31501703, .RightChannelOffset = 31439778, .LeftColorChannel = Green,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_O,      .LeftImageName = "Monument Valley",                       .RightImageName = "Modern highway (Ithaca, New York)",     .LeftChannelOffset = 31986826, .RightChannelOffset = 31939644, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_P,      .LeftImageName = "Forest scene with mushrooms",           .RightImageName = "Golden Gate Bridge",                    .LeftChannelOffset = 32486942, .RightChannelOffset = 32440627, .LeftColorChannel = Red,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_A,      .LeftImageName = "Forest scene with mushrooms",           .RightImageName = "Train (United Aircraft Corporation Turbotrain)", .LeftChannelOffset = 32978679, .RightChannelOffset = 32945392, .LeftColorChannel = Green,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_S,      .LeftImageName = "Forest scene with mushrooms",           .RightImageName = "Airplane in flight",                    .LeftChannelOffset = 33489509, .RightChannelOffset = 33469402, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_D,      .LeftImageName = "Leaf (Fragaria)",                       .RightImageName = "Airport (Toronto)",                     .LeftChannelOffset = 34005577, .RightChannelOffset = 33984056, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_F,      .LeftImageName = "Autumn Fallen leaves",                  .RightImageName = "Antarctic expedition (Commonwealth Trans-Antarctic Expedition)", .LeftChannelOffset = 34523161, .RightChannelOffset = 34490520, .LeftColorChannel = Red,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_G,      .LeftImageName = "Autumn Fallen leaves",                  .RightImageName = "Radio telescope (Westerbork)",          .LeftChannelOffset = 35015433, .RightChannelOffset = 34999599, .LeftColorChannel = Green,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_H,      .LeftImageName = "Autumn Fallen leaves",                  .RightImageName = "Radio telescope (Arecibo)",             .LeftChannelOffset = 35537491, .RightChannelOffset = 35520804, .LeftColorChannel = Blue,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_J,      .LeftImageName = "Snowflakes over Sequoia",               .RightImageName = "Page of book (Newton, On the System of the World)", .LeftChannelOffset = 36022479, .RightChannelOffset = 36038801, .LeftColorChannel = Red,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_K,      .LeftImageName = "Snowflakes over Sequoia",               .RightImageName = "Astronaut in space (Ed White)",         .LeftChannelOffset = 36547852, .RightChannelOffset = 36582381, .LeftColorChannel = Green,  .RightColorChannel = Red},
    {.ShiftKey = KEY_L,      .LeftImageName = "Snowflakes over Sequoia",               .RightImageName = "Astronaut in space (Ed White)",         .LeftChannelOffset = 37076695, .RightChannelOffset = 37093950, .LeftColorChannel = Blue,  .RightColorChannel = Green},
    {.ShiftKey = KEY_Z,      .LeftImageName = "Tree with daffodils",                   .RightImageName = "Astronaut in space (Ed White)",         .LeftChannelOffset = 37669499, .RightChannelOffset = 37654503, .LeftColorChannel = Red,  .RightColorChannel = Blue},
    {.ShiftKey = KEY_X,      .LeftImageName = "Tree with daffodils",                   .RightImageName = "Titan Centaur launch",                  .LeftChannelOffset = 38155490, .RightChannelOffset = 38150712, .LeftColorChannel = Green,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_C,      .LeftImageName = "Tree with daffodils",                   .RightImageName = "Sunset with birds",                     .LeftChannelOffset = 38667959, .RightChannelOffset = 38655546, .LeftColorChannel = Blue,  .RightColorChannel = Red},
    {.ShiftKey = KEY_V,      .LeftImageName = "Flying insect with flowers (Ichneumonidae)", .RightImageName = "Sunset with birds",                .LeftChannelOffset = 39157401, .RightChannelOffset = 39177643, .LeftColorChannel = Mono,  .RightColorChannel = Green},
    {.ShiftKey = KEY_B,      .LeftImageName = "Diagram of vertebrate evolution",       .RightImageName = "Sunset with birds",                     .LeftChannelOffset = 39651843, .RightChannelOffset = 39671543, .LeftColorChannel = Mono,  .RightColorChannel = Blue},
    {.ShiftKey = KEY_N,      .LeftImageName = "Seashell (Xancidae)",                   .RightImageName = "String Quartet (Quartetto Italiano)",   .LeftChannelOffset = 40149135, .RightChannelOffset = 40171212, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
    {.ShiftKey = KEY_M,      .LeftImageName = "Dolphins",                              .RightImageName = "Violin with music score (Cavatina)",    .LeftChannelOffset = 40702862, .RightChannelOffset = 40670528, .LeftColorChannel = Mono,  .RightColorChannel = Mono},
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
            if (SampleOffset > M.LeftChannelOffset && (!bHaveNext || (bHaveNext && SampleOffset < M2.LeftChannelOffset)))
            {
                Result = i;
                break;
            }
        }
        else
        {
            if (SampleOffset > M.RightChannelOffset && (!bHaveNext || (bHaveNext && SampleOffset < M2.RightChannelOffset)))
            {
                Result = i;
                break;
            }
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
            Result = ImageMappings[ChannelIndex].LeftImageName;
        }
        else
        {
            Result = ImageMappings[ChannelIndex].RightImageName;
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
            Result = ImageMappings[ChannelIndex].LeftColorChannel;
        }
        else
        {
            Result = ImageMappings[ChannelIndex].RightColorChannel;
        }
    }

    return Result;
}

u32 LineHeight = 430;

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
    IDLE,
    TRACKING
};

static f32 SampleHistoryLeft[1000] = {0};
static f32 SampleHistoryRight[1000] = {0};

bool FindNegativeTroughPeak(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel, u32* OutIndex, f32* OutValue)
{
    bool bFound = false;

    u32 SamplesCount = Wav.frameCount;

    const f32 FallThreshold = -0.03f;
    const u32 DebounceSamples = 2;
    const f32 NoiseHisteresis = 0.002f;

    u32 State = IDLE;
    u32 StableCount = 0;
    i32 TroughSampleIndex = -1;
    f32 TroughSampleValue = 0.0;

    i32 Confirmed_TroughSampleIndex = -1;
    f32 Confirmed_TroughSampleValue = 0.0;

    // printf("Offset: %u | SearchLength: %u\n", SampleOffset, SearchLength);

    for (u32 i = 0; i < SearchLength; i++)
    {
        if (SampleOffset + i >= SamplesCount)
        {
            break;
        }

        f32 Current  = Samples[(SampleOffset+i)     * Wav.channels + (bLeftChannel ? 0 : 1)];
        f32 Next     = Samples[(SampleOffset+(i+1)) * Wav.channels + (bLeftChannel ? 0 : 1)];
        f32 Diff     = Next - Current;// - Previous;

        switch (State)
        {
            case IDLE:
            {
                // printf("Diff: %f ", Diff);
                if (Diff < FallThreshold)
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

    return bFound;
}

void DetectScanTrigger(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel, i32* OutIndex, f32* OutValue)
{
    bool bFound = false;

    u32 SamplesCount = Wav.frameCount;

    const f32 FallThreshold = 0.04f;
    const u32 DebounceSamples = 3;
    const f32 NoiseHisteresis = 0.001f;

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
        f32 Diff     = fabsf(Next - Current);

        switch (State)
        {
            case IDLE:
            {
                if (Diff > FallThreshold)
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
                      u32* Cursor, f32 Threshold, EImageColorChannel ColorChannel)
{
    bool bSuccess = false;

    u32 SamplesPerLine = (f32)Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack          = 0.05 * (f32)SamplesPerLine;
    u32 NextLinePrediction = (*Cursor + (SamplesPerLine - Slack));

        // printf("Cursor: %u | Prediction: %u | SamplesPerLine: %u\n", *Cursor, NextLinePrediction, SamplesPerLine);
        // printf("Cursor: %u | Prediction: %u\n", *Cursor, NextLinePrediction, SamplesPerLine);

    i32 PeakIndex = -1;
    f32 PeakValue = 0;
    if (bLeftChannel)
    {
        DetectScanTrigger(Samples, NextLinePrediction, Slack*2, Wav, bLeftChannel, &PeakIndex, &PeakValue);
    }
    else
    {
        // TODO: right channgel specific threshold params
        FindNegativeTroughPeak(Samples, NextLinePrediction, Slack*2, Wav, bLeftChannel, &PeakIndex, &PeakValue);
    }

    // printf("%i | %f\n", NextLinePrediction + PeakIndex, PeakValue);

    // f32 BestScore = SyncScore(Samples, NextLinePrediction, Slack, Wav.channels, bLeftChannel);

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
            if (V < 0) V = 0; if (V > 255) V = 255;

            V = 255 - V;

            Color CurrentColor = GetImageColor(*OutputImage, StepIndex, y);
            Color PixelColor = {0};
            switch (ColorChannel)
            {
                default:
                case Mono:
                {
                    PixelColor = (Color){ V, V, V, 255};
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

    Left->ImageOffset = M.LeftChannelOffset;
    Left->Cursor = M.LeftChannelOffset;
    Left->ScanLine = 0;

    Right->ImageOffset = M.RightChannelOffset;
    Right->Cursor = M.RightChannelOffset;
    Right->ScanLine = 0;
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

    u32 StartPointX = BaseLocationX;
    u32 EndPointX   = BaseLocationX + (ScanWidth  * 1.5f - 100);
    u32 MidPointY   = BaseLocationY + (ScanHeight * 1.5f + 65);

    u32 Window      = SamplesPerLine * NumSamplesToDraw;
    u32 CursorStart = MusicCursor - Window;
    u32 CursorEnd   = MusicCursor;

    i32 TriggerRel = 0;
    DetectScanTrigger(Samples, CursorStart, Window, Wav, bLeftChannel, &TriggerRel, NULL);

    if (TriggerRel > 0)
    {
        Window      = CursorEnd - CursorStart;
        CursorStart = MusicCursor - Window;
        CursorEnd   = CursorStart + TriggerRel;
    }

    for (u32 i = CursorStart; i < CursorEnd; i++)
    {
        f32 Value = Samples[i * Wav.channels + Channel] * 350.0f;

        f32 Alpha = ((f32)(CursorEnd - i) / Window);

        u32 XOffset = StartPointX + (i32)((EndPointX - StartPointX) * (1.0f - Alpha));

        DrawPixel(XOffset, MidPointY + Value, WHITE);
    }
}

i32 main(void)
{
    const i32 ScreenWidth = 1600;
    const i32 ScreenHeight = 900;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(ScreenWidth, ScreenHeight, "Golden Decoder");
    SetTargetFPS(0);

    i32 BaseLocationX = GetScreenWidth()/2 - 800;
    i32 BaseLocationY = GetScreenHeight()/2 - 325;

    InitAudioDevice();

    Music GoldenWav = LoadMusicStream("resources/golden.wav");
    Wave Wav = LoadWave("resources/golden.wav");
    PlayMusicStream(GoldenWav);
    
    f32* Samples = LoadWaveSamples(Wav);

    Image Scan_Left = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Left = LoadTextureFromImage(Scan_Left);

    Image Scan_Right = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Right = LoadTextureFromImage(Scan_Right);

    u32 SamplesPerLine = Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack = 0.05 * SamplesPerLine;

    RecordPlayer Player_LeftChannel = {.ScanImage = &Scan_Left, .ScanTexture = ScanTexture_Left};
    RecordPlayer Player_RightChannel = {.ScanImage = &Scan_Right, .ScanTexture = ScanTexture_Right};

    Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, Slack, Wav.channels, true) / Slack;
    Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, Slack, Wav.channels, false) / Slack;
    
    while (!WindowShouldClose())
    {
        UpdateMusicStream(GoldenWav);
        
        u32 NumMappings = sizeof(ImageMappings) / sizeof(ImageMappings[0]);
        for (u32 i = 0; i < NumMappings; i++)
        {
            ImageMapping M = ImageMappings[i];
            bool bIsShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (bIsShift)
            {
                if (IsKeyPressed(M.ShiftKey))
                {
                    JumpToMapping(&Player_LeftChannel, &Player_RightChannel, i);

                    Player_LeftChannel.Threshold  = SyncPeak(Samples, Player_LeftChannel.ImageOffset, SamplesPerLine, Wav.channels, true);
                    Player_RightChannel.Threshold = SyncPeak(Samples, Player_RightChannel.ImageOffset, SamplesPerLine, Wav.channels, false);

                    u64 MusicPosition = M.LeftChannelOffset < M.RightChannelOffset ? M.LeftChannelOffset : M.RightChannelOffset;
                    SeekMusicStream(GoldenWav, (f32)MusicPosition / (f32)Wav.sampleRate);

                    break;
                }
            }
            else
            {
                if (IsKeyPressed(M.Key))
                {
                    // DecodeImage(Wav, M.LeftChannelOffset, &Scan_Left, ScanTexture_Left, true);
                    // DecodeImage(Wav, M.RightChannelOffset, &Scan_Right, ScanTexture_Right, false);
    
                    JumpToMapping(&Player_LeftChannel, &Player_RightChannel, i);
    
                    Player_LeftChannel.Threshold  = SyncPeak(Samples, Player_LeftChannel.ImageOffset, SamplesPerLine, Wav.channels, true);
                    Player_RightChannel.Threshold = SyncPeak(Samples, Player_RightChannel.ImageOffset, SamplesPerLine, Wav.channels, false);
    
                    u64 MusicPosition = M.LeftChannelOffset < M.RightChannelOffset ? M.LeftChannelOffset : M.RightChannelOffset;
                    SeekMusicStream(GoldenWav, (f32)MusicPosition / (f32)Wav.sampleRate);
    
                    break;
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
            if (DecodeImage_StepV2(Samples, Player_LeftChannel.ScanLine, Wav, Player_LeftChannel.ScanImage, Player_LeftChannel.ScanTexture, true, &Player_LeftChannel.Cursor, Player_LeftChannel.Threshold, GetColorChannelFromSampleOffset(Player_LeftChannel.Cursor, true)))
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
            if (DecodeImage_StepV2(Samples, Player_RightChannel.ScanLine, Wav, Player_RightChannel.ScanImage, Player_RightChannel.ScanTexture, false, &Player_RightChannel.Cursor, Player_RightChannel.Threshold, GetColorChannelFromSampleOffset(Player_RightChannel.Cursor, false)))
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

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTextureEx(ScanTexture_Left, (Vector2){BaseLocationX, BaseLocationY}, 0, 1.5f, WHITE);
        DrawTextureEx(ScanTexture_Right, (Vector2){BaseLocationX+800, BaseLocationY}, 0, 1.5f, WHITE);

        // DrawFPS(5, 5);

        f32 PlayedTime = GetMusicTimePlayed(GoldenWav);
        const char* TimeText = TextFormat("%02i:%02i:%03i", (i32)PlayedTime / 60, (i32)PlayedTime % 60, (i32)(PlayedTime * 1000) % 1000);
        i32 TimeFontSize = 30;
        i32 TimeCenterX  = BaseLocationX + (i32)((690 + Scan_Left.width * 1.5f) / 2);
        DrawText(TimeText, TimeCenterX - MeasureText(TimeText, TimeFontSize) / 2, BaseLocationY-50, TimeFontSize, WHITE);

        // DrawText("Left Channel", BaseLocationX+(Scan_Left.width*1.5f)/4, BaseLocationY - 100, 14, WHITE);
        DrawText(GetImageNameFromSampleOffset(Player_LeftChannel.Cursor, true), BaseLocationX+(Scan_Left.width*1.5f)/4, BaseLocationY - 80, 50, WHITE);

        // DrawText("Right Channel", BaseLocationX+800+(Scan_Right.width*1.5f)/4, BaseLocationY - 100, 14, WHITE);
        DrawText(GetImageNameFromSampleOffset(Player_RightChannel.Cursor, false), BaseLocationX+800+(Scan_Right.width*1.5f)/4, BaseLocationY - 80, 50, WHITE);

        static u32 NumSamplesToDraw = 6;
        if (IsKeyPressed(KEY_UP))
        {
            NumSamplesToDraw++;
            // printf("%u\n", NumSamplesToDraw);
        }
        if (IsKeyPressed(KEY_DOWN))
        {
            NumSamplesToDraw--;
            // printf("%u\n", NumSamplesToDraw);
        }

        DrawChannelWaveform(Samples, Wav, MusicCursor, SamplesPerLine, NumSamplesToDraw,
                            true,  BaseLocationX,       BaseLocationY, Scan_Left.width, Scan_Left.height);

        DrawChannelWaveform(Samples, Wav, MusicCursor, SamplesPerLine, NumSamplesToDraw,
                            false, BaseLocationX + 800, BaseLocationY, Scan_Left.width, Scan_Left.height);

        EndDrawing();
    }

    UnloadTexture(ScanTexture_Left);
    UnloadTexture(ScanTexture_Right);
    UnloadImage(Scan_Left);
    UnloadImage(Scan_Right);
    UnloadWaveSamples(Samples);

    CloseWindow();

    return 0;
}
