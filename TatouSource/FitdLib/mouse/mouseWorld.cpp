///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"
#include "mouseGate.h"

#include <algorithm>

namespace mouseworld
{
World g_world;

constexpr int kPlayerStandAnim = 4;         // AITD1 hero stand animation

// Write localJoyD and record that the mouse drove it this frame, so a
// mid-frame takeover (e.g. FoundObjet opened by the hero's touch) can zero a
// stale value before PlayWorld's tank controls read it.
void setJoyD(int value)
{
    localJoyD = value;
    g_world.wroteJoyD = true;
}

// Run InitAnim on the hero (InitAnim works on the "current processed" actor).
void initHeroAnim(int anim, int type, int info)
{
    tObject* savedPtr = currentProcessedActorPtr;
    const int savedIdx = currentProcessedActorIdx;
    currentProcessedActorIdx = currentCameraTargetActor;
    currentProcessedActorPtr = &hero();
    InitAnim(anim, type, info);
    currentProcessedActorPtr = savedPtr;
    currentProcessedActorIdx = savedIdx;
}

// Stop an actor dead: no step, no turn in progress.
void haltActor(tObject& a)
{
    a.speed = 0;
    a.direction = 0;
    a.rotate.numSteps = 0;
}

// Stop the hero where it stands and put it back in its stand pose (FITD anim.cpp:256-267
// commits the pending step when this transition applies).
void stopHero()
{
    if (!heroAvailable())
        return;
    tObject& h = hero();
    haltActor(h);
    // Never force the stand pose over a script-owned hero (the mouse lets go of it) or
    // while an uninterruptable anim (hit/death) is current or queued: GereAnim commits
    // newAnim unconditionally, bypassing InitAnim's ANIM_UNINTERRUPTABLE refusal.
    if (g_gameId == AITD1 && h.trackMode == 1 && !((h.animType | h.newAnimType) & ANIM_UNINTERRUPTABLE))
    {
        initHeroAnim(kPlayerStandAnim, 0, kPlayerStandAnim);
        h.newAnim = kPlayerStandAnim;
        h.newAnimType = 0;
        h.newAnimInfo = kPlayerStandAnim;
    }
}

void cancelIntent()
{
    const bool held = g_world.intent && g_world.intent->requiresHold;
    g_world.intent.reset();
    g_world.decision.reset();
    if (held)
        stopHero();
}

// Aim a live intent somewhere new: plan afresh and forget stall progress.
void retarget(mouse::NavIntent& in, mouse::XZ dest, int room)
{
    in.dest = dest;
    in.room = room;
    in.planned = false;
    mouse::resetStall(in);
}
} // namespace mouseworld

using namespace mouseworld;

namespace
{
mouse::ClickGate s_screenGate;
bool s_worldActive = false;

bool latchedPush()
{
    return g_world.intent && g_world.intent->requiresHold;
}

void startIntent(mouse::ClickKind kind, const mouse::Payload& p, bool run)
{
    mouse::NavIntent in;
    in.dest = mouse::XZ{ p.x, p.z };
    in.room = p.room;
    in.targetObject = p.object;
    in.requiresHold = kind == mouse::ClickKind::Push;
    in.run = run && !in.requiresHold;          // leaning on furniture is never a run
    in.steering = kind == mouse::ClickKind::Steer;
    g_world.push = HeldPush{};
    if (in.requiresHold)
        g_world.push.originRoom = hero().room;
    g_world.intent = in;
    g_world.intentFloor = g_currentFloor;
    g_world.actionSent = false;
}

void applyDecision(const mouse::Decision& d)
{
    switch (d.type)
    {
    case mouse::DecisionType::OpenHud:
        // The opened screen's first quick click is not a fullscreen double-click.
        menuNoteItemClick();
        localKey = hudKeyFor(d.kind);
        break;
    case mouse::DecisionType::Issue:
        startIntent(d.kind, d.payload, d.run);
        break;
    case mouse::DecisionType::Cancel:
        cancelIntent();
        break;
    case mouse::DecisionType::Attack:
        armAttack(d.payload.actor);
        break;
    default:
        break;
    }
}

void endPointerHold()
{
    mouse::endHold(g_world.pointer, g_world.intent && g_world.intent->steering);
    cancelIntent();
}

void releaseAll()
{
    mouse::resetPointer(g_world.pointer);
    cancelIntent();
    clearAttack();
}

// Not driving the world this frame (option off, cutscene, no hero).
void leaveWorld()
{
    s_worldActive = false;
    if (g_world.intent || g_world.attackTarget >= 0 || g_world.pointer.held)
        releaseAll();
    mouseInputRequestCursor(mouse::CursorShape::Default);
}

// The actor of a clicked world object, or null once it is taken or gone.
const tObject* targetActor(int worldIdx)
{
    const tWorldObject* w = worldObject(worldIdx);
    if (!w || w->objIndex < 0 || w->objIndex >= NUM_MAX_OBJECT)
        return nullptr;
    return &ListObjets[w->objIndex];
}

// Beside a clicked object: lean into it, so the engine's own collision
// (anim.cpp) opens FoundObjet for a pickable object exactly as in keyboard
// play, and a found script can see the touch. False when the object is gone.
bool beginContact(mouse::NavIntent& in)
{
    const tObject* a = targetActor(in.targetObject);
    if (!a)
        return false;
    in.engaged = true; // decide() no longer arrives by distance
    retarget(in, mouse::XZ{ a->roomX, a->roomZ }, a->room);
    return true;
}

// One frame of leaning into a clicked object. False when the intent ended.
bool tickContact()
{
    const tObject* a = targetActor(g_world.intent->targetObject);
    if (!a || g_world.actionSent)
    {
        cancelIntent(); // gone, or its Action went out last frame
        return false;
    }
    if (a->COL_BY != currentCameraTargetActor)
        return true; // not touching yet (COL_BY holds last frame's collisions)
    if (a->objectType & AF_FOUNDABLE)
    {
        cancelIntent(); // the engine opened FoundObjet on the touch, or refused it
        return false;
    }
    // A found script: one frame of Action while still leaning in, so the
    // object's life sees the touch and the Action together.
    localClick = 1; // PlayWorld turns this into action = 0x2000
    g_world.actionSent = true;
    return true;
}

// An arrived or abandoned decision: it neither advances nor holds the stick.
void handleArrival(const mouse::NavDecision& d)
{
    mouse::NavIntent& in = *g_world.intent;
    if (in.requiresHold)
    {
        if (d.arrived && !in.engaged)
        {
            in.engaged = true; // standing at the face: from now on, lean into it
            refreshHeldTarget();
        }
        else
            cancelIntent(); // pushed as far as it goes, or stuck
        return;
    }
    if (d.arrived && in.targetObject >= 0 && !in.engaged && beginContact(in))
        return;
    cancelIntent(); // a floor walk arrived, or the object could not be touched
}

void tickNavigation(uint32_t now)
{
    g_world.decision.reset();
    if (!g_world.intent)
        return;
    mouse::NavIntent& in = *g_world.intent;
    const tObject& h = hero();
    if (in.requiresHold && h.room != g_world.push.originRoom)
    {
        cancelIntent();
        return;
    }
    if (in.requiresHold && !refreshHeldTarget())
        return;
    if (!in.requiresHold && in.engaged && !tickContact())
        return;
    mouse::NavEnv env;
    env.grid = gridFor(h.room, agentOf(h));
    env.linkMidpoint = [](int from, int to) { return linkMidpoint(from, to); };
    env.reframe = [](mouse::XZ p, int from, int to) { return mouse::reframe(p, originOf(from), originOf(to)); };
    env.capObjet = [](int x1, int z1, int beta, int x2, int z2) { return CapObjet(x1, z1, beta, x2, z2); };
    const mouse::NavDecision d = mouse::decide(in, heroPose(), env, now);
    g_world.decision = d;
    setJoyD(d.joyd); // LIFE scripts reading the stick see a live one
    if (d.arrived || d.abandoned)
        handleArrival(d);
}

// Resolve what is under the pointer for the cursor; hovering never changes state.
void updateHover()
{
    if (!g_world.hoverPos)
    {
        g_world.hover = {};
        mouseInputRequestCursor(mouse::CursorShape::Default);
        return;
    }
    if (g_world.pointer.held && latchedPush())
        g_world.hover = mouse::ClickResult{ mouse::ClickKind::Push, {} }; // the push cursor sticks while held
    else
        g_world.hover = resolveAt(*g_world.hoverPos);
    mouseInputRequestCursor(kindInfo(g_world.hover.kind).cursor);
}
}

void mouseWorldTakeOver()
{
    s_worldActive = false;
    s_screenGate.arm();
    releaseAll();
    if (g_world.wroteJoyD)
    {
        // A mid-frame takeover (e.g. FoundObjet opened by the hero's touch, or
        // a script's screen, while the mouse holds the stick) must not leave
        // it live for the tank controls this same frame reads it.
        localJoyD = 0;
        g_world.wroteJoyD = false;
    }
    mouseInputRequestCursor(mouse::CursorShape::Default);
    menuRestoreCursorForMenu(); // every screen opened from play shows the cursor
}

bool mouseWorldIsActive()
{
    return s_worldActive;
}

bool mouseScreenClickFilter(bool clickedThisFrame, bool downNow)
{
    return s_screenGate.filter(clickedThisFrame, downNow);
}

void mouseWorldFloorChanged()
{
    clearGeometryCaches();
}

void mouseWorldFrame(int allowSystemMenu)
{
    g_world.wroteJoyD = false; // cleared at the start of every frame
    g_world.allowSystemMenu = allowSystemMenu;
    mouse::Frame frame;
    const bool haveFrame = mouseInputTakeFrame(&frame);

    // Cutscenes and intros: a left click skips exactly like the Action key.
    if (!allowSystemMenu)
    {
        leaveWorld();
        if (menuMouseSkipClicked())
            localClick = 1;
        return;
    }
    if (!g_remasterConfig.controls.mouseGameplay || NumCamera < 0 || !heroAvailable())
    {
        leaveWorld();
        return;
    }
    s_worldActive = true;
    if (frame.blocked)
    {
        releaseAll(); // F1 dialog or an ImGui window owns the mouse
        mouseInputRequestCursor(mouse::CursorShape::Default);
        return;
    }

    const uint32_t now = (uint32_t)SDL_GetTicks();
    const int camera = NumCamera;
    const mouse::Resolver resolve = [](mouse::Point p) { return resolveAt(p); };

    // The floor changed under a walk.
    if (g_world.intentFloor != g_currentFloor)
    {
        if (g_world.pointer.held)
            mouse::rebase(g_world.pointer);
        cancelIntent();
        g_world.intentFloor = g_currentFloor;
    }

    for (const mouse::Event& e : frame.events)
    {
        switch (e.type)
        {
        case mouse::EventType::Motion:
            g_world.lastInputMouse = true;
            mouse::onMove(g_world.pointer, e.pos);
            break;
        case mouse::EventType::Down:
            g_world.lastInputMouse = true;
            mouse::onPress(g_world.pointer, e.pos);
            if (e.pos)
                applyDecision(mouse::pressDecision(g_world.pointer, *e.pos, e.clicks, camera, resolve, latchedPush()));
            break;
        case mouse::EventType::Up:
            endPointerHold();
            break;
        case mouse::EventType::FocusLost:
            releaseAll();
            break;
        }
    }
    // A release SDL never delivered: the button is up now.
    if (haveFrame && g_world.pointer.held && !frame.leftDown)
        endPointerHold();

    const std::optional<mouse::Point> pointerNow = haveFrame ? frame.pos : g_world.pointer.pos;

    // Held pointer follow: once per frame, re-resolving only when it moved.
    if (g_world.pointer.held)
        applyDecision(mouse::holdDecision(g_world.pointer, pointerNow, camera, resolve, latchedPush(),
                                          g_world.intent.has_value()));

    // Every walk is hold-bound.
    if (g_world.intent && !g_world.pointer.held)
        cancelIntent();

    // A script took the hero (cutscene walk, death): drop the walk and the
    // swing, and spend the hold so only a new press moves the hero once it is
    // handed back.
    if (hero().trackMode != 1)
    {
        cancelIntent();
        clearAttack();
        if (g_world.pointer.held)
            g_world.pointer.spent = true;
    }

    // An accepted enemy click owns the hero until the swing finishes.
    if (!tickAttack(now))
        tickNavigation(now);
    g_world.hoverPos = pointerNow;
    updateHover();
}

void mouseWorldKeyboardTookOver()
{
    g_world.lastInputMouse = false;
    if (g_world.intent || g_world.attackTarget >= 0)
    {
        cancelIntent();
        clearAttack();
    }
    if (g_world.pointer.held)
        g_world.pointer.spent = true; // no follow resumes on this hold
}

bool mouseNavSteer(tObject* actor)
{
    if (!s_worldActive || !heroAvailable() || actor != &hero())
        return false;
    if (g_world.attackTarget < 0 && !g_world.intent)
        return false;
    // During a swing forward is held for the LIFE script's melee, not to walk;
    // a walk with no step this frame stops dead, like this fork's tank controls.
    if (g_world.attackTarget >= 0 || !g_world.decision || !g_world.decision->advance)
    {
        haltActor(*actor);
        return true;
    }
    const mouse::NavDecision& d = *g_world.decision;
    turnActorToward(actor, d.target.x, d.target.z); // follow mode's turn, aimed at the waypoint
    actor->speed = d.run ? 5 : 4; // 5 is FITD's run speed
    return true;
}

bool mouseWorldHudState(MouseHudState* out)
{
    *out = MouseHudState{};
    if (!s_worldActive || !g_world.allowSystemMenu)
        return false;
    for (int i = 0; i < mouse::kHudIconCount; ++i)
        out->iconEnabled[i] = hudIconAllowed((mouse::HudIcon)i);
    out->pointer = g_world.hoverPos;
    if (g_world.hoverPos)
        out->hoverIcon = mouse::hudIconAt(*g_world.hoverPos);
    out->held = g_world.pointer.held;
    out->settling = mouse::settling(g_world.pointer);
    if (g_world.intent && !g_world.intent->steering)
        out->destination = screenOf(g_world.intent->room, g_world.intent->dest);
    const mouse::ClickKind k = g_world.hover.kind;
    if (!g_world.pointer.held && !out->destination && (k == mouse::ClickKind::Walk || k == mouse::ClickKind::Target))
        out->preview = screenOf(g_world.hover.payload.room, mouse::XZ{ g_world.hover.payload.x, g_world.hover.payload.z });
    return true;
}

bool mouseWorldWantsCursor()
{
    return g_remasterConfig.controls.mouseGameplay && g_world.lastInputMouse;
}
