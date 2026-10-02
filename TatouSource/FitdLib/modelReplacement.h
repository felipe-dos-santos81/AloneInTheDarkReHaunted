///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: draws an animated body as its replacement mesh
// (models_hd/body_<HQR>_<NNN>.hdm, written by `make import-models`) in place
// of the classic primitives. Everything the game computes from a body — pose,
// collision, picking, shadows, lamp glow, actor->screen* — stays on the
// classic body; only drawing changes. Engine-coupled: the only HD-model file
// that touches bgfx or engine globals.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

struct sBody;
struct ModelReplacement;

// graphics.hdModels is on and the game is AITD1.
bool hdModelsActive();

// The replacement to draw for this body, or nullptr to draw classic. The
// first call per body loads and checks its file once (a missing file is
// silent; a rejected one logs one line); later calls only look it up.
ModelReplacement* findModelReplacement(int bodyNum, sBody* pBody, const std::string& hqrName);

// Draws the replacement with the body's current pose, as AffObjet would draw
// the classic body at (x, y, z) turned by (alpha, beta, gamma). Returns false,
// having drawn nothing, when the classic body must be drawn instead.
bool drawModelReplacement(ModelReplacement* r, sBody* pBody, int x, int y, int z, int alpha, int beta, int gamma);

// The 320x200 box [x0, y0, x1, y1] the last AffObjet's replacement covered;
// false when it drew the classic body. AffObjet forgets it at its start, and
// drawBgOverlay once it has used it.
bool lastReplacementBox(int box[4]);
void forgetReplacementBox();

// .hdm files read so far: stays flat during play (nothing loads per frame).
int modelReplacementLoads();

// Destroys every GPU resource (cleanupAndExit).
void shutdownModelReplacements();
