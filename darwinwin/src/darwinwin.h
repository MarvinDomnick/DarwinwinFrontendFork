#pragma once

#include "core.h"
#include "neural_net.h"
#include "evolution.h"

enum tileFlag : uint8_t
{
  tf_Underwater = 1,
  tf_Protein = 1 << 1,
  tf_Sugar = 1 << 2,
  tf_Vitamin = 1 << 3,
  tf_Fat = 1 << 4,
  tf_Collidable = 1 << 5,
  tf_OtherActor = 1 << 6, // not on the map
  tf_Hidden = 1 << 7, // not on the map
};

void tileFlag_toTempString(const uint8_t flag, char(&out)[9]);
void tileFlag_print(const uint8_t flag);

struct level
{
  static constexpr size_t width = 32;
  static constexpr size_t height = 32;

  static constexpr uint8_t wallThickness = 3; // this needs a shorter name

  uint8_t grid[width * height];
};

void level_initLinear(level *pLevel);
void level_print(const level &level);

struct actor;

bool level_performStep1(level &lvl, actor &actor);
bool level_performStep2(level &lvl, actor *pActors);
bool level_performStep3(level &lvl, actor *pActors);
bool level_performStep4(level &lvl, actor *pActors);

enum lookDirection
{
  ld_left,
  ld_up,
  ld_right,
  ld_down,

  _lookDirection_Count,
};

const char *lookDirection_name(const lookDirection dir);

enum viewConePosition
{
  vcp_self,
  vcp_nearLeft,
  vcp_nearCenter,
  vcp_nearRight,
  vcp_midLeft,
  vcp_midCenter,
  vcp_midRight,
  vcp_farCenter,

  _viewConePosition_Count,
};

struct viewCone
{
  uint8_t values[_viewConePosition_Count];

  uint8_t operator [](const viewConePosition pos) const
  {
    lsAssert(pos < LS_ARRAYSIZE(values));
    return values[pos];
  }
};

viewCone viewCone_get(const level &lvl, const actor &actor);
void viewCone_print(const viewCone &values, const actor &actor);

enum actorStats
{
  as_Energy,
  as_Air,
  as_Protein,
  as_Sugar,
  as_Vitamin,
  as_Fat,

  _actorStats_Count,
};

struct actor
{
  vec2u16 pos;
  lookDirection look_at_dir;
  bool alive;
  uint8_t stats[_actorStats_Count];
  neural_net<(_viewConePosition_Count * 8 + _actorStats_Count + (neural_net_block_size - 1)) / neural_net_block_size, 4> brain;

  actor(const vec2u8 pos, const lookDirection dir) : pos(pos), look_at_dir(dir) { lsAssert(pos.x >= level::wallThickness && pos.x < (level::width - level::wallThickness) && pos.y >= level::wallThickness && pos.y < (level::height - level::wallThickness)); }
};

enum actorAction
{
  aa_Move,
  aa_Move2,
  aa_TurnLeft,
  aa_TurnRight,
  aa_Eat,

  _actorAction_Count
};

void actor_updateStats(actor *pActor, const viewCone &cone);
void actor_act(actor *pActor, level *pLevel, const viewCone &cone, const actorAction action);
