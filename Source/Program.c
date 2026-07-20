#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "Clock.h"

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

typedef struct
{
    KeyboardKey Key;
    const char* LeftImageName;
    const char* RightImageName;
    u32 LeftChannelOffset;
    u32 RightChannelOffset;
} ChannelMapping;


typedef struct
{
    u32 ImageOffset;
    f32 Threshold;
    u32 Cursor;
    u32 ScanLine;
    Image* ScanImage;
    Texture2D ScanTexture;
    // bool bFinished;
} RecordPlayer;

// TODO: make these clickable
const u32 LeftChannel_Circle   = 687981;
const u32 LeftChannel_MilkyWay = 952854;
const u32 LeftChannel_Math_1   = 1223649;
const u32 LeftChannel_Math_2   = 1484927;
const u32 LeftChannel_Math_3   = 1743594;

const u32 RightChannel_BeachR   = 745392;
const u32 RightChannel_BeachG   = 992388;
const u32 RightChannel_BeachB   = 1252857;
const u32 RightChannel_Frog     = 1516492;
const u32 RightChannel_Lizard   = 1773431;

#define OFFSET_SCALING 2
#define SAMPLES_FACTOR (1.0f/(120.0f/(f32)(OFFSET_SCALING)))

ChannelMapping ChannelMappings[] =
{
    {.Key = KEY_ONE,   .LeftImageName = "Calibration Circle", .RightImageName = "Beach (Red)",   .LeftChannelOffset = 687981  * OFFSET_SCALING, .RightChannelOffset = 745392  * OFFSET_SCALING},
    {.Key = KEY_TWO,   .LeftImageName = "Milky Way",          .RightImageName = "Beach (Green)", .LeftChannelOffset = 952854  * OFFSET_SCALING, .RightChannelOffset = 992388  * OFFSET_SCALING},
    {.Key = KEY_THREE, .LeftImageName = "Math 1",             .RightImageName = "Beach (Blue)",  .LeftChannelOffset = 1223649 * OFFSET_SCALING, .RightChannelOffset = 1252857 * OFFSET_SCALING},
    {.Key = KEY_FOUR,  .LeftImageName = "Math 2",             .RightImageName = "Frog",          .LeftChannelOffset = 1484927 * OFFSET_SCALING, .RightChannelOffset = 1516492 * OFFSET_SCALING},
    {.Key = KEY_FIVE,  .LeftImageName = "Math 3",             .RightImageName = "Lizard",        .LeftChannelOffset = 1743594 * OFFSET_SCALING, .RightChannelOffset = 1773431 * OFFSET_SCALING},
    {.Key = KEY_SIX,   .LeftImageName = "Math 4",             .RightImageName = "Bird",          .LeftChannelOffset = 1999655 * OFFSET_SCALING, .RightChannelOffset = 2026229 * OFFSET_SCALING},
};

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

    // u32 SamplesPerLine = Wav.sampleRate * (1.0/120.0);
    u32 SamplesCount   = Wav.frameCount / Wav.channels;

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

void DetectScanTrigger(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel, u32* OutIndex, f32* OutValue)
{
    bool bFound = false;

    // u32 SamplesPerLine = Wav.sampleRate * (1.0/120.0);
    u32 SamplesCount   = Wav.frameCount / Wav.channels;

    const f32 FallThreshold = -0.025f;
    const u32 DebounceSamples = 3;
    const f32 NoiseHisteresis = 0.002f;

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
        f32 Diff     = Next - Current;

        switch (State)
        {
            case IDLE:
            {
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
}

bool DetectBeepV2(f32* Samples, u32 SampleOffset, u32 SearchLength, Wave Wav, bool bLeftChannel)
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
                      u32* Cursor, f32 Threshold)
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

    bool bIsBeep = DetectBeepV2(Samples, *Cursor, SamplesPerLine, Wav, bLeftChannel);

    if (bIsBeep)
    {
        printf("Beep!\n");
    }

    f32 Diff = fabsf(BestScore - Threshold);
    bool bWithinBand = Diff < 0.1f;
    // bool bWithinBand = Threshold / BestScore > 0.1f;
    printf("Best: %f | Thresold: %f\n", BestScore, Threshold);
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
    ChannelMapping M = ChannelMappings[Index];

    Left->ImageOffset = M.LeftChannelOffset;
    Left->Cursor = M.LeftChannelOffset;
    Left->ScanLine = 0;

    Right->ImageOffset = M.RightChannelOffset;
    Right->Cursor = M.RightChannelOffset;
    Right->ScanLine = 0;
}

i32 main(void)
{
    const i32 ScreenWidth = 1600;
    const i32 ScreenHeight = 900;

    InitWindow(ScreenWidth, ScreenHeight, "Golden Decoder");
    SetTargetFPS(120);

    InitAudioDevice();

    Music GoldenWav = LoadMusicStream("resources/golden.wav");
    Wave Wav = LoadWave("resources/golden.wav");
    PlayMusicStream(GoldenWav);
    
    f32* Samples = LoadWaveSamples(Wav);

    Image Scan_Left = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Left = LoadTextureFromImage(Scan_Left);

    Image Scan_Right = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Right = LoadTextureFromImage(Scan_Right);
    
    // DecodeImage(Wav, LeftChannel_Circle, &Scan_Left, ScanTexture_Left, true);
    // DecodeImage(Wav, RightChannel_Frog, &Scan_Right, ScanTexture_Right, false);

    i32 BaseLocationX = GetScreenWidth()/2 - 800;
    i32 BaseLocationY = GetScreenHeight()/2 - 300;

    u32 CurrentChannelIndex = 0;

    f64 SyncBurstWidth = 10.0;
    // f64 SamplesPerLine = 379.0;
    u32 SamplesPerLine = Wav.sampleRate * SAMPLES_FACTOR;
    u32 Slack = 0.05 * SamplesPerLine;

    /*

    u64 StartFrame = ImageSampleOffset;

    // TODO: fix leak
    f32* Samples = LoadWaveSamples(Wav);

    f64 Score = SyncScore(Samples, StartFrame, SyncBurstWidth, Wav.channels, bLeftChannel);
    // printf("Burst Score: %f\n", Score);
    
    f64 Cursor = StartFrame;
    f64 Threshold = Score * 0.5;
    u64 Line = 0;
    */

    RecordPlayer Player_LeftChannel = {.ScanImage = &Scan_Left, .ScanTexture = ScanTexture_Left};
    RecordPlayer Player_RightChannel = {.ScanImage = &Scan_Right, .ScanTexture = ScanTexture_Right};

    // JumpToMapping(&Player_LeftChannel, &Player_RightChannel, 0);

    Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, Slack, Wav.channels, true) / Slack;
    Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, Slack, Wav.channels, false) / Slack;

    u32 NumMappings = sizeof(ChannelMappings) / sizeof(ChannelMappings[0]);

    f32 StartWave = SyncScore(Samples, 200000, Slack, Wav.channels, true) / Slack;
    printf("Start wave Score: %f (x2 %f)\n", StartWave, StartWave * 2);
    f32 AvgSpikeyWave_1 = SyncScore(Samples, 437374, Slack, Wav.channels, true) / Slack;
    printf("Spikey wave Score: %f (x2 %f)\n", AvgSpikeyWave_1, AvgSpikeyWave_1 - StartWave);
    f32 AvgScreechWave_1 = SyncScore(Samples, 985055, Slack, Wav.channels, true) / Slack;
    printf("Screech wave Score: %f (x2 %f)\n", AvgScreechWave_1, AvgScreechWave_1 - AvgSpikeyWave_1);
    f32 AvgBeepWave_1 = SyncScore(Samples, 1366553, Slack, Wav.channels, true) / Slack;
    printf("Beep wave Score: %f (x2 %f)\n", AvgBeepWave_1, AvgBeepWave_1 - AvgScreechWave_1);
    f32 FirstImage_1 = SyncScore(Samples, 1383413, Slack, Wav.channels, true) / Slack;
    printf("First Image wave Score: %f (x2 %f)\n", FirstImage_1, FirstImage_1 - AvgBeepWave_1);
    // f32 RandImage_1 = SyncScore(Samples, 3433363, 1000, Wav.channels, true) / 1000;
    // printf("Rand Image wave Score: %f\n", RandImage_1);
    // f32 AvgBeepWave_2 = SyncScore(Samples, 3539094, 1000, Wav.channels, true) / 1000;
    // printf("Beep wave 2 Score: %f\n", AvgBeepWave_2);
    // f32 AvgBlankWave_1 = SyncScore(Samples, 896063, 50000, Wav.channels, true) / 50000.0f;
    // printf("blank Score: %f\n", AvgBlankWave_1);


    /*
    {
        i32 Index = 0;
        f32 Value = 0;
        u32 StartingOffset = 0;//ChannelMappings[0].LeftChannelOffset;
        while (1)
        {
            if (FindNegativeTroughPeak(Samples, StartingOffset, SamplesPerLine, Wav, true, &Index, &Value))
            {
                printf("Negative Peak Index: %d (%u) | %f\n", Index, StartingOffset+Index, Value);
            }

            StartingOffset += Wav.sampleRate * (1.0/120.0);
        }
    }
    */

    /*
    {
        i32 Index = 0;
        f32 Value = 0;
        u32 StartingOffset = ChannelMappings[0].LeftChannelOffset;
        u32 count = 0;
        while (count < 100)
        {
            if (FindPositivePeak(Samples, StartingOffset, SamplesPerLine, Wav, true, &Index, &Value))
            {
                printf("Positive Peak Index: %d (%u) | %f\n", Index, StartingOffset+Index, Value);
            }

            StartingOffset += Wav.sampleRate * SAMPLES_FACTOR;
            count++;
        }
    }
    */

    while (!WindowShouldClose())
    {
        UpdateMusicStream(GoldenWav);

        for (u32 i = 0; i < NumMappings; i++)
        {
            ChannelMapping M = ChannelMappings[i];
            if (IsKeyPressed(M.Key))
            {
                // DecodeImage(Wav, M.LeftChannelOffset, &Scan_Left, ScanTexture_Left, true);
                // DecodeImage(Wav, M.RightChannelOffset, &Scan_Right, ScanTexture_Right, false);

                CurrentChannelIndex = i;

                JumpToMapping(&Player_LeftChannel, &Player_RightChannel, i);

                // Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, Slack, Wav.channels, true);
                // Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, Slack, Wav.channels, false);
                Player_LeftChannel.Threshold  = SyncPeak(Samples, Player_LeftChannel.ImageOffset, SamplesPerLine, Wav.channels, true);
                Player_RightChannel.Threshold = SyncPeak(Samples, Player_RightChannel.ImageOffset, SamplesPerLine, Wav.channels, false);


                u64 MusicPosition = M.LeftChannelOffset < M.RightChannelOffset ? M.LeftChannelOffset : M.RightChannelOffset;
                SeekMusicStream(GoldenWav, (f32)MusicPosition / (f32)Wav.sampleRate);

                break;
            }
        }

        f32 MusicCursor = (f32)GetMusicTimePlayed(GoldenWav) * (f32)Wav.sampleRate;

        {
            f32 Peak = SyncPeak(Samples, Player_LeftChannel.Cursor, SamplesPerLine, Wav.channels, true);
            Player_LeftChannel.Threshold = Peak/SamplesPerLine;
        }

        while (Player_LeftChannel.Cursor + SamplesPerLine <= MusicCursor)
        {
            if (DecodeImage_StepV2(Samples, Player_LeftChannel.ScanLine, Wav, Player_LeftChannel.ScanImage, Player_LeftChannel.ScanTexture, true, &Player_LeftChannel.Cursor, Player_LeftChannel.Threshold))
            {
                Player_LeftChannel.ScanLine++;
            }
            else
            {
                Player_LeftChannel.ScanLine = 0;
                Player_LeftChannel.Cursor = MusicCursor;
                // printf("left finished\n");
                break;
            }
        }

        {

            f32 Peak = SyncPeak(Samples, Player_RightChannel.Cursor, SamplesPerLine, Wav.channels, false);
            Player_RightChannel.Threshold = Peak/SamplesPerLine;
        }


        while (Player_RightChannel.Cursor + SamplesPerLine <= MusicCursor)
        {
            if (DecodeImage_StepV2(Samples, Player_RightChannel.ScanLine, Wav, Player_RightChannel.ScanImage, Player_RightChannel.ScanTexture, false, &Player_RightChannel.Cursor, Player_RightChannel.Threshold))
            {
                Player_RightChannel.ScanLine++;
            }
            else
            {
                Player_RightChannel.ScanLine = 0;
                Player_RightChannel.Cursor = MusicCursor;
                // Player_RightChannel.bFinished = true;
                break;
            }
        }

        UpdateTexture(Player_RightChannel.ScanTexture, Player_RightChannel.ScanImage->data);
        UpdateTexture(Player_LeftChannel.ScanTexture, Player_LeftChannel.ScanImage->data);

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTextureEx(ScanTexture_Left, (Vector2){BaseLocationX, BaseLocationY}, 0, 1.5f, WHITE);
        DrawTextureEx(ScanTexture_Right, (Vector2){BaseLocationX+800, BaseLocationY}, 0, 1.5f, WHITE);

        DrawText("Left Channel", BaseLocationX, BaseLocationY - 120, 14, WHITE);
        DrawText(ChannelMappings[CurrentChannelIndex].LeftImageName, BaseLocationX, BaseLocationY - 100, 50, WHITE);

        DrawText("Right Channel", BaseLocationX+800, BaseLocationY - 120, 14, WHITE);
        DrawText(ChannelMappings[CurrentChannelIndex].RightImageName, BaseLocationX+800, BaseLocationY - 100, 50, WHITE);

        // DrawCircle((BaseLocationX+(267*1.5)), (BaseLocationY+((LineHeight/2.0 * 1.5) - 45)), 220, WHITE);

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
