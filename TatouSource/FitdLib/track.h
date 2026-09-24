///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Actor movement tracking declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _TRACK_
#define _TRACK_

void processTrack(void);
void GereManualRot(int param);
void resetTrackStuckCounters(int actorIdx);
int CapObjet(int x1, int z1, int beta, int x2, int z2);
char* getRoomLink(unsigned int room1, unsigned int room2);
// Centre of the zone linking room1 to room2, in room1's frame.
void getRoomLinkCenter(unsigned int room1, unsigned int room2, int* x, int* y, int* z);
// Follow mode's turn: rotate `actor` toward (x, z) of its own room's frame.
struct tObject;
void turnActorToward(tObject* actor, int x, int z);

#endif
