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
    u64 LeftChannelOffset;
    u64 RightChannelOffset;
} ChannelMapping;


typedef struct
{
    u64 ImageOffset;
    f64 Threshold;
    f64 Cursor;
    u32 ScanLine;
    Image* ScanImage;
    Texture2D ScanTexture;
    bool bFinished;
} RecordPlayer;

// TODO: make these clickable
const u64 LeftChannel_Circle   = 687981;
const u64 LeftChannel_MilkyWay = 952854;
const u64 LeftChannel_Math_1   = 1223649;
const u64 LeftChannel_Math_2   = 1484927;
const u64 LeftChannel_Math_3   = 1743594;

const u64 RightChannel_BeachR   = 745392;
const u64 RightChannel_BeachG   = 992388;
const u64 RightChannel_BeachB   = 1252857;
const u64 RightChannel_Frog     = 1516492;
const u64 RightChannel_Lizard   = 1773431;

ChannelMapping ChannelMappings[] =
{
    {.Key = KEY_ONE,   .LeftImageName = "Calibration Circle", .RightImageName = "Beach (Red)",   .LeftChannelOffset = 687981,  .RightChannelOffset = 745392},
    {.Key = KEY_TWO,   .LeftImageName = "Milky Way",          .RightImageName = "Beach (Green)", .LeftChannelOffset = 952854,  .RightChannelOffset = 992388},
    {.Key = KEY_THREE, .LeftImageName = "Math 1",             .RightImageName = "Beach (Blue)",  .LeftChannelOffset = 1223649, .RightChannelOffset = 1252857},
    {.Key = KEY_FOUR,  .LeftImageName = "Math 2",             .RightImageName = "Frog",          .LeftChannelOffset = 1484927, .RightChannelOffset = 1516492},
    {.Key = KEY_FIVE,  .LeftImageName = "Math 3",             .RightImageName = "Lizard",        .LeftChannelOffset = 1743594, .RightChannelOffset = 1773431},
    {.Key = KEY_SIX,   .LeftImageName = "Math 4",             .RightImageName = "Bird",          .LeftChannelOffset = 1999655, .RightChannelOffset = 2026229},
};

u32 LineHeight = 384;

f64 SyncScore(f32* Samples, u64 TestIndex, u64 SampleWidth, u32 Channels, bool bLeft)
{
    f64 Result = 0;

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

bool DecodeImage_Step(f32* Samples, u64 StepIndex, Wave Wav, u64 ImageSampleOffset, Image* OutputImage, Texture2D OutputTexture, bool bLeftChannel,
                      f64* Cursor, u64 Threshold)
{
    bool bSuccess = false;

    f64 SyncBurstWidth = 10.0;
    f64 SamplesPerLine = 379.0;
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

    f64 Cursor = StartFrame;
    f64 Threshold = Score * 0.5;
    for (u64 Line = 0; Line < 2000; Line++)
    {
        if (DecodeImage_Step(Samples, Line, Wav, ImageSampleOffset, OutputImage, OutputTexture, bLeftChannel, &Cursor, Threshold))
        {
            // UpdateTexture(OutputTexture, OutputImage->data);
        }
        else
        {
            break;
        }

        /*
        f64 NextLinePrediction = Cursor + SamplesPerLine;
        
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
            f64 NextLineSamplesActual = (f64)(BestIndex - Cursor);
            f64 Delta = NextLineSamplesActual - SamplesPerLine;
            // printf("line %llu: %f (delta: %f)\n", Line, NextLineSamplesActual, Delta);

            f64 Black = -0.15;
            f64 White = 0.1;

            f64 ImageStart = Cursor - SyncBurstWidth;
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
                ImageDrawPixel(OutputImage, Line, y, PixelColor);
            }

            // Cursor = (f64)BestIndex;
            Cursor = (f64)Edge;
        }
        else
        {
            break;
        }
        */
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

    Music GoldenWav = LoadMusicStream("resources/golden2x-32.wav");
    Wave Wav = LoadWave("resources/golden2x-32.wav");
    PlayMusicStream(GoldenWav);
    
    f32* Samples = LoadWaveSamples(Wav);

    Image Scan_Left = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Left = LoadTextureFromImage(Scan_Left);

    Image Scan_Right = GenImageColor(600, LineHeight, BLANK);
    Texture2D ScanTexture_Right = LoadTextureFromImage(Scan_Right);
    

    // TODO: make these sliders
    /*
    f64 HighPitchWidth = 8192.0;
    f64 SyncBurstWidth = 20.0;
    f64 SyncBurstScoreThreshold = 400.0;
    f64 SamplesPerLine = 370.0;
    f64 Slack = 0.1 * SamplesPerLine;
    
    
    u64 StartFrame_L = LeftChannel_Circle + HighPitchWidth;
    // u64 StartFrame_L = 1227257;// + HighPitchWidth;
    // f32 SampleLeft = Samples[(StartFrameLeft + i) * Wav.channels + 0];
    // f32* SamplesRight = &Samples[StartFrameRight * Wav.channels + 1];

    f32* SyncBurstStart = Samples + StartFrame_L;
    // f32* SyncBurstEnd   = Samples + ((StartFrameLeft + (u64)SyncWidth_Samples) * Wav.channels) + 0;

    f64 Score = SyncScore(SyncBurstStart, 0, SyncBurstWidth, Wav.channels, true);
    printf("Burst Score: %f\n", Score);

    f64 Cursor = StartFrame_L;
    f64 Threshold = Score * 0.5;
    for (u64 Line = 0; Line < 2000; Line++)
    {
        f64 NextLinePrediction = Cursor + SamplesPerLine;
        
        u64 SearchStart = (u64)(NextLinePrediction - Slack);
        u64 SearchEnd   = (u64)(NextLinePrediction + Slack);

        // now find the next peak (best score out of our search window)
        u64 BestIndex = 0;
        f64 BestScore = -1.0;
        for (u64 i = SearchStart; i < SearchEnd; i++)
        {
            f64 Score = SyncScore(Samples, i, SyncBurstWidth, Wav.channels, true);
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
            f32 A = fabsf(Samples[i * Wav.channels + 0]);
            if (A > Peak) Peak = A;
        }

        // walk left from the coarse hit, then forward to the first 50%-of-peak crossing
        u64 Edge = BestIndex - (u64)SyncBurstWidth;
        while (fabsf(Samples[Edge * Wav.channels + 0]) < Peak * 0.5f)
        {
            Edge++;
        }


        if (BestScore >= Threshold)
        {
            f64 NextLineSamplesActual = (f64)(BestIndex - Cursor);
            f64 Delta = NextLineSamplesActual - SamplesPerLine;
            printf("line %llu: %f (delta: %f)\n", Line, NextLineSamplesActual, Delta);

            f64 Black = -0.10;
            f64 White = 0.1;

            f64 ImageStart = Cursor - SyncBurstWidth;
            f64 ImageLen   = NextLineSamplesActual;

            for (i32 y = 0; y < LineHeight; y++)
            {
                u64 SampleIndex0 = (u64)ImageStart + ((f64)y     / (f64)LineHeight) * ImageLen;
                u64 SampleIndex1 = (u64)ImageStart + ((f64)(y+1) / (f64)LineHeight) * ImageLen;
                if (SampleIndex1 <= SampleIndex0) { SampleIndex1 = SampleIndex0 + 1; }

                f64 Sum = 0;
                for (u64 i = SampleIndex0; i < SampleIndex1; i++)
                {
                    Sum += Samples[i * Wav.channels + 0];
                }
                f64 Avg = Sum/(f64)(SampleIndex1-SampleIndex0);

                i32 V = (i32)((Avg - Black) / (White - Black) * 255.0);
                if (V < 0) V = 0; if (V > 255) V = 255;

                V = 255 - V;

                Color PixelColor = (Color){ V, V, V, 255};
                ImageDrawPixel(&Scan_Left, Line, y, PixelColor);
            }

            // Cursor = (f64)BestIndex;
            Cursor = (f64)Edge;
        }
        else
        {
            break;
        }
    }

    UpdateTexture(ScanTexture_Left, Scan_Left.data);
    UpdateTexture(ScanTexture_Right, Scan_Right.data);
    */

    // DecodeImage(Wav, LeftChannel_Circle, &Scan_Left, ScanTexture_Left, true);
    // DecodeImage(Wav, RightChannel_Frog, &Scan_Right, ScanTexture_Right, false);

    i32 BaseLocationX = GetScreenWidth()/2 - 800;
    i32 BaseLocationY = GetScreenHeight()/2 - 300;

    u32 CurrentChannelIndex = 0;

    f64 SyncBurstWidth = 10.0;
    f64 SamplesPerLine = 379.0;
    f64 Slack = 0.1 * SamplesPerLine;

    u8 Substep = 2;
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

    JumpToMapping(&Player_LeftChannel, &Player_RightChannel, 0);

    Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, SyncBurstWidth, Wav.channels, true) * 0.5;
    Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, SyncBurstWidth, Wav.channels, false) * 0.5;

    u32 NumMappings = sizeof(ChannelMappings) / sizeof(ChannelMappings[0]);

    while (!WindowShouldClose())
    {
        UpdateMusicStream(GoldenWav);

        f64 MusicCursor = (f64)GetMusicTimePlayed(GoldenWav) * (f64)Wav.sampleRate;

        for (u32 i = 0; i < NumMappings; i++)
        {
            ChannelMapping M = ChannelMappings[i];
            if (IsKeyPressed(M.Key))
            {
                // DecodeImage(Wav, M.LeftChannelOffset, &Scan_Left, ScanTexture_Left, true);
                // DecodeImage(Wav, M.RightChannelOffset, &Scan_Right, ScanTexture_Right, false);

                CurrentChannelIndex = i;

                JumpToMapping(&Player_LeftChannel, &Player_RightChannel, i);

                Player_LeftChannel.Threshold = SyncScore(Samples, Player_LeftChannel.ImageOffset, SyncBurstWidth, Wav.channels, true) * 0.5;
                Player_RightChannel.Threshold = SyncScore(Samples, Player_RightChannel.ImageOffset, SyncBurstWidth, Wav.channels, false) * 0.5;


                u64 MusicPosition = M.LeftChannelOffset < M.RightChannelOffset ? M.LeftChannelOffset : M.RightChannelOffset;
                SeekMusicStream(GoldenWav, (f64)MusicPosition / (f64)Wav.sampleRate);

                break;
            }
        }

        // for (u8 i = 0; i < Substep; i++)
        {
            // if (!Player_LeftChannel.bFinished)
            while (!Player_LeftChannel.bFinished && Player_LeftChannel.Cursor + SamplesPerLine <= MusicCursor)
            {
                if (DecodeImage_Step(Samples, Player_LeftChannel.ScanLine, Wav, Player_LeftChannel.ImageOffset, Player_LeftChannel.ScanImage, Player_LeftChannel.ScanTexture, true, &Player_LeftChannel.Cursor, Player_LeftChannel.Threshold))
                {
                    Player_LeftChannel.ScanLine++;
                }
                else
                {
                    // Player_LeftChannel.ScanLine = 0;
                    Player_LeftChannel.bFinished = true;
                    // break;
                }
            }
    
            // if (!Player_RightChannel.bFinished)
            while (!Player_RightChannel.bFinished && Player_RightChannel.Cursor + SamplesPerLine <= MusicCursor)
            {
                if (DecodeImage_Step(Samples, Player_RightChannel.ScanLine, Wav, Player_RightChannel.ImageOffset, Player_RightChannel.ScanImage, Player_RightChannel.ScanTexture, false, &Player_RightChannel.Cursor, Player_RightChannel.Threshold))
                {
                    Player_RightChannel.ScanLine++;
                }
                else
                {
                    // Player_RightChannel.ScanLine = 0;
                    Player_RightChannel.bFinished = true;
                    // break;
                }
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
