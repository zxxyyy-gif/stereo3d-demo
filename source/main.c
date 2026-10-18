/*
 * stereo3d-demo —— 3DS 裸眼立体（分层视差）最小示例
 *
 * 做的事：把一场 2D 横版画面拆成 7 个不同"深度"的图层，
 * 每帧给上屏画两遍（左眼 / 右眼），每层的水平偏移 = 深度 x 滑块值 x 强度。
 * 滑块推到 0 时两眼完全重合，画面就是普通 2D；推满时近层明显凸出。
 *
 * 整个效果的核心只有两行：
 *   gfxSet3D(true);
 *   每层偏移 ox = eye * depth * STEREO_MAX_PX * gfxGet3DLevel();
 *
 * 编译：见 README.md（需要 devkitPro + 3ds-dev 包）
 */

#include <3ds.h>
#include <citro2d.h>
#include <math.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

/* 滑块推满时，深度 1.0 的图层在单眼上的最大偏移（像素）。
 * 12px 意味着双眼总分离 24px，在 400px 宽的屏上已经相当"凸"。
 * 觉得晕就调小到 8，觉得没感觉就调到 16。 */
#define STEREO_MAX_PX 12.0f

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

/* 图层深度：0 = 贴在屏幕最深处（两眼完全重合），1 = 最靠前（视差最大） */
#define D_SKY    0.00f
#define D_CLOUD  0.10f
#define D_HILL   0.22f
#define D_TREE   0.40f
#define D_GROUND 0.60f
#define D_PLAYER 0.90f
#define D_FG     1.00f

/* 世界参数 */
#define GROUND_Y  176.0f
#define TILE_W    20.0f

static C3D_RenderTarget* topLeft;
static C3D_RenderTarget* topRight;
static C3D_RenderTarget* bottom;

static float camX    = 600.0f;
static float playerX = 600.0f;

static u32 clrSky, clrCloud, clrHillFar, clrHillNear, clrTrunk, clrCanopy;
static u32 clrGround, clrGrass, clrPlayer, clrSkin, clrFg;
static u32 clrUiBg, clrAccent, clrKnob, clrTrack;

static float wrapf(float v, float period)
{
	float r = fmodf(v, period);
	if (r < 0.0f) r += period;
	return r;
}

/* 单眼偏移：eye = +1 左眼，-1 右眼。正值向左眼方向位移 -> 产生凸出感 */
static inline float parallax(float depth, float eye, float level)
{
	return eye * depth * STEREO_MAX_PX * level;
}

/* 地形高度（单位：方块数），确定性函数，保证两眼的几何完全一致 */
static int colHeight(int c)
{
	float h = 2.0f + 1.7f * sinf(c * 0.55f) + 0.8f * sinf(c * 1.63f + 1.2f);
	int n = (int)lroundf(h);
	if (n < 0) n = 0;
	if (n > 4) n = 4;
	return n;
}

static void draw_scene(float eye, float level)
{
	/* 天空：深度 0，两眼完全一样，等于"无限远"的底 */
	C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, clrSky);

	/* 云 */
	{
		const float period = 220.0f, scroll = 0.18f;
		float ox = parallax(D_CLOUD, eye, level);
		float base = wrapf(-camX * scroll, period);
		for (int i = -1; i <= 3; i++) {
			float sx = base + i * period;
			C2D_DrawRectSolid(sx + ox, 34, 0, 84, 11, clrCloud);
			C2D_DrawRectSolid(sx + 16 + ox, 25, 0, 52, 10, clrCloud);
			C2D_DrawRectSolid(sx + 30 + ox, 44, 0, 60, 8, clrCloud);
		}
	}

	/* 远山，两层叠出纵深感 */
	{
		const float period = 260.0f, scroll = 0.42f;
		float ox = parallax(D_HILL, eye, level);
		float base = wrapf(-camX * scroll, period);
		for (int i = -1; i <= 3; i++) {
			float sx = base + i * period;
			C2D_DrawTriangle(sx + ox, GROUND_Y, clrHillFar,
			                 sx + 70 + ox, 106.0f, clrHillFar,
			                 sx + 150 + ox, GROUND_Y, clrHillFar, 0);
			C2D_DrawTriangle(sx + 100 + ox, GROUND_Y, clrHillNear,
			                 sx + 182 + ox, 128.0f, clrHillNear,
			                 sx + 254 + ox, GROUND_Y, clrHillNear, 0);
		}
	}

	/* 中景树 */
	{
		const float period = 150.0f, scroll = 0.70f;
		float ox = parallax(D_TREE, eye, level);
		float base = wrapf(-camX * scroll, period);
		for (int i = -1; i <= 4; i++) {
			float sx = base + i * period;
			C2D_DrawRectSolid(sx + 40 + ox, 130, 0, 8, 46, clrTrunk);
			C2D_DrawRectSolid(sx + 16 + ox, 100, 0, 56, 34, clrCanopy);
			C2D_DrawRectSolid(sx + 26 + ox, 86, 0, 36, 20, clrCanopy);
		}
	}

	/* 地形（方块层） */
	{
		float ox = parallax(D_GROUND, eye, level);
		int c0 = (int)floorf(camX / TILE_W);
		for (int c = c0 - 1; c <= c0 + 21; c++) {
			float sx = (float)c * TILE_W - camX + ox;
			int h = colHeight(c);
			float top = GROUND_Y - (float)h * TILE_W;
			C2D_DrawRectSolid(sx, top, 0, TILE_W, 240.0f - top, clrGround);
			C2D_DrawRectSolid(sx, top, 0, TILE_W, 5, clrGrass);
		}
	}

	/* 角色：比地面更靠前，所以立体感上会"浮"在砖块之上 */
	{
		float ox = parallax(D_PLAYER, eye, level);
		int c = (int)floorf((playerX + 9.0f) / TILE_W);
		float top = GROUND_Y - (float)colHeight(c) * TILE_W;
		float sx = playerX - camX - 9.0f + ox;
		float sy = top - 34.0f;
		C2D_DrawRectSolid(sx, sy + 8, 0, 18, 26, clrPlayer);
		C2D_DrawRectSolid(sx + 4, sy, 0, 10, 10, clrSkin);
	}

	/* 前景草：最靠前，滑块推满时会明显"贴脸" */
	{
		const float period = 46.0f, scroll = 1.28f;
		float ox = parallax(D_FG, eye, level);
		float base = wrapf(-camX * scroll, period);
		for (int i = -1; i <= 10; i++) {
			float sx = base + i * period;
			C2D_DrawRectSolid(sx + ox, 228, 0, 7, 12, clrFg);
			C2D_DrawRectSolid(sx + 14 + ox, 232, 0, 5, 8, clrFg);
		}
	}
}

/* 下屏：滑块读数 + 各层双眼分离量示意 */
static void draw_ui(float level)
{
	C2D_DrawRectSolid(0, 0, 0, BOT_W, BOT_H, clrUiBg);
	C2D_DrawRectSolid(0, 0, 0, BOT_W, 3, clrAccent);

	const u32 layerClr[5]   = { clrCloud, clrHillFar, clrTrunk, clrGrass, clrFg };
	const float layerDepth[5] = { D_CLOUD, D_HILL, D_TREE, D_GROUND, D_FG };

	for (int i = 0; i < 5; i++) {
		float off = layerDepth[i] * level * 16.0f;
		C2D_DrawRectSolid(58.0f + i * 48.0f + off, 84, 0, 16, 16, layerClr[i]);
		C2D_DrawRectSolid(58.0f + i * 48.0f - off, 112, 0, 16, 16, layerClr[i]);
	}

	/* 3D 滑块 */
	C2D_DrawRectSolid(40, 190, 0, 240, 8, clrTrack);
	C2D_DrawRectSolid(40, 190, 0, 240.0f * level, 8, clrAccent);
	C2D_DrawCircleSolid(40.0f + 240.0f * level, 194, 0, 9, clrKnob);
}

int main(int argc, char** argv)
{
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

	/* 手动建三个 target：上屏左右眼各一个，下屏一个。
	 * 不用 C2D_Prepare()，因为它只会给上屏建一个默认 target。 */
	topLeft = C3D_RenderTargetCreate(TOP_H, TOP_W, GPU_RB_DEPTH24_STENCIL8, GPU_RB_RGBA8);
	C3D_RenderTargetSetOutput(topLeft, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	topRight = C3D_RenderTargetCreate(TOP_H, TOP_W, GPU_RB_DEPTH24_STENCIL8, GPU_RB_RGBA8);
	C3D_RenderTargetSetOutput(topRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	bottom = C3D_RenderTargetCreate(BOT_H, BOT_W, GPU_RB_DEPTH24_STENCIL8, GPU_RB_RGBA8);
	C3D_RenderTargetSetOutput(bottom, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	/* 打开立体模式：上屏进入视差屏障工作状态，右眼 buffer 才会被显示 */
	gfxSet3D(true);

	clrSky      = C2D_Color32(0x8C, 0xC5, 0xE6, 0xFF);
	clrCloud    = C2D_Color32(0xFA, 0xFC, 0xFF, 0xFF);
	clrHillFar  = C2D_Color32(0x86, 0xAE, 0x9B, 0xFF);
	clrHillNear = C2D_Color32(0x5F, 0x88, 0x74, 0xFF);
	clrTrunk    = C2D_Color32(0x6B, 0x4B, 0x33, 0xFF);
	clrCanopy   = C2D_Color32(0x4E, 0x8A, 0x4A, 0xFF);
	clrGround   = C2D_Color32(0x6B, 0x53, 0x3E, 0xFF);
	clrGrass    = C2D_Color32(0x6E, 0xA5, 0x4A, 0xFF);
	clrPlayer   = C2D_Color32(0xE0, 0x6C, 0x3C, 0xFF);
	clrSkin     = C2D_Color32(0xF2, 0xC9, 0x9B, 0xFF);
	clrFg       = C2D_Color32(0x3F, 0x6B, 0x35, 0xFF);
	clrUiBg     = C2D_Color32(0x20, 0x22, 0x26, 0xFF);
	clrAccent   = C2D_Color32(0xF0, 0xB0, 0x50, 0xFF);
	clrKnob     = C2D_Color32(0xFF, 0xE6, 0xB0, 0xFF);
	clrTrack    = C2D_Color32(0x3A, 0x3D, 0x42, 0xFF);

	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START) break;

		circlePosition cp;
		hidCircleRead(&cp);
		float ax = (float)cp.dx / 156.0f;
		if (kHeld & KEY_DLEFT)  ax = -1.0f;
		if (kHeld & KEY_DRIGHT) ax =  1.0f;

		playerX += ax * 3.0f;
		if (playerX < 40.0f)    playerX = 40.0f;
		if (playerX > 3600.0f)  playerX = 3600.0f;
		camX += (playerX - camX) * 0.10f;

		float level = gfxGet3DLevel();
		if (level < 0.0f) level = 0.0f;
		if (level > 1.0f) level = 1.0f;

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		C2D_TargetClear(topLeft, clrSky);
		C2D_SceneBegin(topLeft);
		draw_scene(+1.0f, level);

		C2D_TargetClear(topRight, clrSky);
		C2D_SceneBegin(topRight);
		draw_scene(-1.0f, level);

		C2D_TargetClear(bottom, clrUiBg);
		C2D_SceneBegin(bottom);
		draw_ui(level);

		C3D_FrameEnd(0);
	}

	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
