/*
 * Copyright 2011 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Code: Cyril Meynier
//
// Copyright (c) 1999-2000 ARKANE Studios SA. All rights reserved

#ifndef ARX_GAME_PLAYER_H
#define ARX_GAME_PLAYER_H

#include <string>
#include <vector>

#include "game/Character.h"
#include "game/Spells.h"
#include "game/Runes.h"
#include "game/Keyring.h"
#include "game/Playerflags.h"

#include "graphics/data/Mesh.h"
#include "math/MathFwd.h"

enum constants
{
	PLAYER_BASE_RADIUS = 52,
	PLAYER_BASE_HEIGHT = -170,
	PLAYER_CROUCH_HEIGHT = -120,
	PLAYER_LEVITATE_HEIGHT = -195,
};

// Quests Management (QuestLogBook)
struct STRUCT_QUEST {
	std::string ident;
	std::string localised;
};

extern ARX_NECKLACE necklace;
extern EERIE_3DOBJ * hero;
extern ANIM_HANDLE * herowaitbook;
extern ANIM_HANDLE * herowait2;
extern ANIM_HANDLE * herowait_2h;
extern std::vector<STRUCT_QUEST> PlayerQuest;

extern arx::keyring Keyring;

extern float DeadCameraDistance;
extern long BLOCK_PLAYER_CONTROLS;
extern long USE_PLAYERCOLLISIONS;
extern long WILLRETURNTOCOMBATMODE;

void ARX_PLAYER_LoadHeroAnimsAndMesh();

void ARX_PLAYER_BecomesDead();

void ARX_PLAYER_ClickedOnTorch(INTERACTIVE_OBJ * io);

void ARX_PLAYER_RectifyPosition();

void ARX_PLAYER_Frame_Update();

void ARX_PLAYER_Manage_Movement();
void ARX_PLAYER_Manage_Death();

void ARX_PLAYER_GotoAnyPoly();

void ARX_PLAYER_Quest_Add(const std::string & quest, bool _bLoad = false);
void ARX_PLAYER_Quest_Init();

void ARX_PLAYER_FrontPos(Vec3f * pos);

void ARX_PLAYER_FrameCheck(float _framedelay);

void ARX_PLAYER_Manage_Visual();

void ARX_PLAYER_Reset_Fall();

void ARX_PLAYER_KillTorch();

void ARX_PLAYER_PutPlayerInNormalStance(long val);

void ARX_PLAYER_Start_New_Quest();
 
void ARX_PLAYER_Restore_Skin();

float GetPlayerStealth();

void ARX_GAME_Reset(long type = 0);
void Manage_sp_max();

bool ARX_PLAYER_IsInFightMode();

void ForcePlayerLookAtIO(INTERACTIVE_OBJ * io);

#endif // ARX_GAME_PLAYER_H