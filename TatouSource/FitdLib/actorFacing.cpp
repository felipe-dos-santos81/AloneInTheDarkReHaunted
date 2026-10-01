///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Stop an actor and turn it toward another at once (see actorFacing.h).
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "actorFacing.h"

void haltActor(tObject& a)
{
    a.speed = 0;
    a.direction = 0;
    a.rotate.numSteps = 0;
}

void faceActorToward(tObject& actor, const tObject& target)
{
    int x = target.roomX;
    int z = target.roomZ;
    const int rooms = (int)roomDataTable.size();
    if (target.room != actor.room && target.room >= 0 && target.room < rooms && actor.room >= 0 && actor.room < rooms)
    {
        // Same re-frame as mouse::reframe (mousePick.cpp); room origins are in tens of units.
        const roomDataStruct& from = roomDataTable[target.room];
        const roomDataStruct& to = roomDataTable[actor.room];
        x -= 10 * (to.worldX - from.worldX);
        z += 10 * (to.worldZ - from.worldZ);
    }
    for (int step = 0; step < 256; ++step)
    {
        const int direction = CapObjet(actor.roomX + actor.stepX, actor.roomZ + actor.stepZ, actor.beta, x, z);
        if (direction == 0)
            break;
        actor.beta = (actor.beta - direction * 4) & 0x3FF;
    }
    haltActor(actor);
}
