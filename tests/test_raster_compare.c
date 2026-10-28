#include "raylib.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <stdlib.h>
extern void *swGetColorBuffer(int *width, int *height);
int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    FILE *out=fopen(argv[1],"wb"); if (!out) return 1;
    SetTraceLogLevel(LOG_NONE);
    InitWindow(X4_WIDTH,X4_HEIGHT,"raster equivalence"); if (!IsWindowReady()) return 1;
    SetTargetFPS(0);
    for (int scene=0; scene<32; scene++) {
        SetRandomSeed(731u+scene);
        BeginDrawing(); ClearBackground((Color){scene*7,scene*7,scene*7,255});
        if (scene%3 == 0) BeginScissorMode(37,43,389,681);
        for (int i=0; i<50; i++) {
            int x=GetRandomValue(-80,470), y=GetRandomValue(-30,790);
            Color c={(unsigned char)GetRandomValue(0,255),(unsigned char)GetRandomValue(0,255),
                (unsigned char)GetRandomValue(0,255),(unsigned char)GetRandomValue(0,255)};
            DrawRectangle(x,y,GetRandomValue(1,190),GetRandomValue(1,80),c);
            DrawTextEx(GetFontDefault(),"X4 Aa 09!?",(Vector2){x,y},10+(i%9)*3.5f,1.25f,c);
        }
        DrawTextEx(GetFontDefault(),"Clip \xc3\xa9",(Vector2){-10,10},23.5f,1.25f,BLACK);
        Camera2D camera={.offset={125,240},.target={40,20},.rotation=scene%2 ? 15:0,.zoom=1.25f};
        BeginMode2D(camera);
        DrawText("Camera",20,30,24,BLACK); DrawCircle(40,80,30,BLACK);
        DrawTriangle((Vector2){10,10},(Vector2){30,90},(Vector2){80,20},BLACK);
        EndMode2D();
        if (scene%3 == 0) EndScissorMode();
        EndDrawing();
        if (fwrite(swGetColorBuffer(NULL,NULL),1,X4_GRAY_BYTES,out) != X4_GRAY_BYTES) return 1;
    }
    CloseWindow(); return fclose(out) != 0;
}
