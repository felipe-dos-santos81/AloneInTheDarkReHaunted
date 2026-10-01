///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"
#include "mouseGate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    in.targetZone = p.zone;
    in.requiresHold = kind == mouse::ClickKind::Push;
    in.run = run && !in.requiresHold;          // leaning on furniture is never a run
    in.steering = kind == mouse::ClickKind::Steer;
    in.exit = kind == mouse::ClickKind::Exit;
    g_world.push = HeldPush{};
    if (in.requiresHold)
        g_world.push.originRoom = hero().room;
    g_world.intent = in;
    g_world.intentFloor = g_currentFloor;
    if (g_remasterConfig.debug.mouseNavOverlay) // MTRACE (temporary)
        printf("MTRACE intent kind=%s dest=(%d,%d) room=%d object=%d run=%d hero=(%d,%d) room=%d\n", kindInfo(kind).name,
               in.dest.x, in.dest.z, in.room, in.targetObject, (int)in.run, heroPose().at.x, heroPose().at.z, hero().room);
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
    g_world.actionHold.reset();
}

// Not driving the world this frame (option off, cutscene, no hero).
void leaveWorld()
{
    s_worldActive = false;
    if (g_world.intent || g_world.attackTarget >= 0 || g_world.pointer.held || g_world.actionHold)
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
    if (in.targetZone >= 0)
    {
        // Furniture painted into the background: lean into its hard col, whose
        // parameter the hero's HARD_COL then reads.
        const mouse::XZ at = heroPose().at;
        auto box = furnitureBox(hero().room, in.targetZone, at);
        if (!box)
            return false;
        in.engaged = true;
        retarget(in, mouse::XZ{ std::clamp(at.x, box->x1, box->x2), std::clamp(at.z, box->z1, box->z2) }, hero().room);
        return true;
    }
    const tObject* a = targetActor(in.targetObject);
    if (!a)
        return false;
    in.engaged = true; // decide() no longer arrives by distance
    retarget(in, mouse::XZ{ a->roomX, a->roomZ }, a->room);
    return true;
}

// Whether the hero touches the intent's object (its COL_BY) or furniture (the
// hero's HARD_COL), as last frame's collisions left them.
bool touchingTarget(const mouse::NavIntent& in)
{
    if (in.targetZone >= 0)
        return hero().HARD_COL == in.targetZone;
    const tObject* a = in.targetObject >= 0 ? targetActor(in.targetObject) : nullptr;
    return a && a->COL_BY == currentCameraTargetActor;
}

// One frame of leaning into a clicked object. False when the intent ended.
bool tickContact(uint32_t now)
{
    const mouse::NavIntent& in = *g_world.intent;
    const tObject* a = in.targetZone >= 0 ? nullptr : targetActor(in.targetObject);
    if ((in.targetZone < 0 && !a) || g_world.actionSent)
    {
        cancelIntent(); // gone, or its Action went out last frame
        return false;
    }
    if (!touchingTarget(in))
        return true; // not touching yet
    if (a && (a->objectType & AF_FOUNDABLE))
    {
        cancelIntent(); // the engine opened FoundObjet on the touch, or refused it
        return false;
    }
    // A found script, or scripted scenery with an inventory action armed:
    // Action while still leaning in, so the object's life sees the touch and
    // the Action together; tickActionHold keeps it held through the gesture.
    localClick = 1; // PlayWorld turns this into action = 0x2000
    g_world.actionSent = true;
    g_world.actionHold = ActionHold{ hero().ANIM, now };
    if (g_remasterConfig.debug.mouseNavOverlay) // MTRACE (temporary)
        printf("MTRACE contact: Action sent on touch of world %d zone %d, hero anim %d\n", a ? a->indexInWorld : -1,
               in.targetZone, hero().ANIM);
    return true;
}

// Keep Action held, like the key, while the hero plays the animation it
// started: the hero's life skips its move handling only while the button is
// held, and that handling would cut an interruptible gesture (the attic
// trunk's open) short before the object's script sees it end.
void tickActionHold(uint32_t now)
{
    if (!g_world.actionHold)
        return;
    const tObject& h = hero();
    const ActionHold& hold = *g_world.actionHold;
    if (mouse::holdAction(hold.animAtAction, h.ANIM, h.flagEndAnim != 0, now - hold.sinceMs))
    {
        localClick = 1;
        return;
    }
    if (g_remasterConfig.debug.mouseNavOverlay) // MTRACE (temporary)
        printf("MTRACE contact: Action released, hero anim %d ended=%d after %u ms\n", h.ANIM, (int)h.flagEndAnim,
               now - hold.sinceMs);
    g_world.actionHold.reset();
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
    if (d.arrived && (in.targetObject >= 0 || in.targetZone >= 0) && !in.engaged && beginContact(in))
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
    if (!in.requiresHold && in.engaged && !tickContact(now))
        return;
    mouse::NavEnv env;
    env.grid = gridFor(h.room, agentOf(h));
    env.linkMidpoint = [](int from, int to) { return linkMidpoint(from, to); };
    env.reframe = [](mouse::XZ p, int from, int to) { return mouse::reframe(p, originOf(from), originOf(to)); };
    env.capObjet = [](int x1, int z1, int beta, int x2, int z2) { return CapObjet(x1, z1, beta, x2, z2); };
    mouse::HeroPose pose = heroPose();
    pose.touchingTarget = touchingTarget(in);
    const mouse::NavDecision d = mouse::decide(in, pose, env, now);
    g_world.decision = d;
    if (g_remasterConfig.debug.mouseNavOverlay) // MTRACE (temporary)
    {
        static uint32_t s_lastMs = 0;
        static mouse::XZ s_lastAt{};
        const mouse::XZ at = heroPose().at;
        if (d.arrived || d.abandoned || now - s_lastMs >= 250)
        {
            const double wx = d.target.x - at.x, wz = d.target.z - at.z, wn = std::hypot(wx, wz);
            const double mx = at.x - s_lastAt.x, mz = at.z - s_lastAt.z, mn = std::hypot(mx, mz);
            printf("MTRACE nav hero=(%d,%d) beta=%d dest=(%d,%d) waypoints=%zu first=(%d,%d) want=(%.2f,%.2f) moved=(%.2f,%.2f)/%.0f "
                   "joyd=0x%x adv=%d arrived=%d abandoned=%d\n",
                   at.x, at.z, h.beta, in.dest.x, in.dest.z, in.waypoints.size(), d.target.x, d.target.z, wn ? wx / wn : 0.0,
                   wn ? wz / wn : 0.0, mn ? mx / mn : 0.0, mn ? mz / mn : 0.0, mn, d.joyd, (int)d.advance, (int)d.arrived,
                   (int)d.abandoned);
            s_lastMs = now;
            s_lastAt = at;
        }
    }
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
    const mouse::Resolver resolve = [](mouse::Point p) {
        g_traceResolve = g_remasterConfig.debug.mouseNavOverlay; // MTRACE (temporary): clicks only, never hover
        const mouse::ClickResult r = resolveAt(p);
        g_traceResolve = false;
        return r;
    };

    // The floor changed under a walk.
    if (g_world.intentFloor != g_currentFloor)
    {
        if (g_world.pointer.held)
            mouse::rebase(g_world.pointer);
        cancelIntent();
        g_world.actionHold.reset();
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
        g_world.actionHold.reset();
        if (g_world.pointer.held)
            g_world.pointer.spent = true;
    }

    tickActionHold(now); // before tickContact, which starts a hold this frame
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
    g_world.actionHold.reset(); // the keyboard holds its own Action
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
    if (!g_world.pointer.held && !out->destination && mouse::isWalkLike(k) && k != mouse::ClickKind::Steer)
        out->preview = screenOf(g_world.hover.payload.room, mouse::XZ{ g_world.hover.payload.x, g_world.hover.payload.z });
    return true;
}

bool mouseWorldWantsCursor()
{
    return g_remasterConfig.controls.mouseGameplay && g_world.lastInputMouse;
}
