#include "darwinwin.h"

void actor_move(actor *pActor, const level &lvl);
void actor_moveTwo(actor *pActor, const level &lvl);
void actor_turnLeft(actor *pActor);
void actor_turnRight(actor *pActor);
void actor_eat(actor *pActor, level *pLvl, const viewCone &cone);

const char *lookDirection_toName[] =
{
  "left",
  "up",
  "right",
  "down",
};

static_assert(LS_ARRAYSIZE(lookDirection_toName) == _lookDirection_Count);

void tileFlag_toTempString(const uint8_t flag, char(&out)[9])
{
  const char lut[9] = "UPSVFCOH";

  for (size_t i = 0, mask = 1; i < 8; mask <<= 1, i++)
    out[i] = (flag & mask) ? lut[i] : ' ';

  out[LS_ARRAYSIZE(out) - 1] = '\0';
}

void tileFlag_print(const uint8_t flag)
{
  char tmp[9];
  tileFlag_toTempString(flag, tmp);
  print(tmp);
}

const char *lookDirection_name(const lookDirection dir)
{
  lsAssert(dir < LS_ARRAYSIZE(lookDirection_toName));
  return lookDirection_toName[dir];
}

void level_initLinear(level *pLevel)
{
  for (size_t i = 0; i < level::width * level::height; i++)
    pLevel->grid[i] = (uint8_t)(i);

  // Making the Borders collidable
  for (size_t i = 0; i < level::width; i++)
  {
    pLevel->grid[i] = tf_Collidable;
    pLevel->grid[i + level::width] = tf_Collidable;
    pLevel->grid[i + level::width * 2] = tf_Collidable;

    pLevel->grid[i + level::width * level::height - 4 * level::width + 1] = tf_Collidable;
    pLevel->grid[i + level::width * level::height - 3 * level::width + 1] = tf_Collidable;
    pLevel->grid[i + level::width * level::height - 2 * level::width + 1] = tf_Collidable;
  }

  for (size_t i = 0; i < level::height; i++)
  {
    pLevel->grid[i * level::width] = tf_Collidable;
    pLevel->grid[i * level::width + 1] = tf_Collidable;
    pLevel->grid[i * level::width + 2] = tf_Collidable;

    pLevel->grid[i * level::width + level::width] = tf_Collidable;
    pLevel->grid[i * level::width + level::width - 1] = tf_Collidable;
    pLevel->grid[i * level::width + level::width - 2] = tf_Collidable;
  }
}

void level_print(const level &level)
{
  print("Level \n");

  for (size_t y = 0; y < level::height; y++)
  {
    for (size_t x = 0; x < level::width; x++)
      print(FU(Min(4))(level.grid[y * level::width + x]));

    print('\n');
  }
}

template <size_t actor_count>
bool level_performStep(level &lvl, actor *pActors)
{
  // TODO: optional level internal step. (grow plants, etc.)

  bool anyAlive = false;

  for (size_t i = 0; i < actor_count; i++)
  {
    if (!pActors[i].alive)
      continue;

    anyAlive = true;

    const viewCone cone = viewCone_get(lvl, pActors[i]);
    actor_updateStats(&pActors[i], cone);

    neural_net_buffer<decltype(actor::brain)::layer_blocks> ioBuffer;

    for (size_t j = 0; j < LS_ARRAYSIZE(cone.values); j++)
      for (size_t bit = 1; bit < 256; bit <<= 1)
        ioBuffer.data[j] = (int8_t)(cone[(viewConePosition)j] & bit);

    neural_net_buffer_prepare(ioBuffer, (LS_ARRAYSIZE(cone.values) * 8) / ioBuffer.block_size);

    // TOOD: Copy over other values (air, health, energy, ... into `inBuffer[LS_ARRAYSIZE(cone.values) * 8 + x]`.

    neural_net_eval(pActors->brain, ioBuffer);

    int16_t maxValue = ioBuffer.data[0];
    size_t bestActionIndex = 0;
    constexpr size_t maxActionIndex = lsMin(LS_ARRAYSIZE(ioBuffer.data), _actorAction_Count);

    for (size_t actionIndex = 1; actionIndex < maxActionIndex; actionIndex++)
    {
      if (maxValue < ioBuffer.data[actionIndex])
      {
        maxValue = ioBuffer.data[actionIndex];
        bestActionIndex = actionIndex;
      }
    }

    actor_act(&pActors[i], &lvl, cone, (actorAction)bestActionIndex);
  }

  lsAssert(anyAlive); // otherwise, maybe don't call us???

  return anyAlive;
}

bool level_performStep1(level &lvl, actor &actor) { return level_performStep<1>(lvl, &actor); }
bool level_performStep2(level &lvl, actor *pActors) { return level_performStep<2>(lvl, pActors); }
bool level_performStep3(level &lvl, actor *pActors) { return level_performStep<3>(lvl, pActors); }
bool level_performStep4(level &lvl, actor *pActors) { return level_performStep<4>(lvl, pActors); }

///////////////////////////////////////////////////////////////////////////////////////////

viewCone viewCone_get(const level &lvl, const actor &a)
{
  lsAssert(a.pos.x > 0 && a.pos.y < level::width);

  viewCone ret;

  size_t currentIdx = a.pos.y * level::width + a.pos.x;
  constexpr ptrdiff_t width = (ptrdiff_t)level::width;
  static const ptrdiff_t lut[_lookDirection_Count][LS_ARRAYSIZE(ret.values)] = {
    { 0, width - 1, -1, -width - 1, width - 2, -2, -width - 2, -3 },
    { 0, -width - 1, -width, -width + 1, -width * 2 - 1, -width * 2, -width * 2 + 1, -width * 3 },
    { 0, -width + 1, 1, width + 1, -width + 2, 2, width + 2, 3 },
    { 0, width + 1, width, width - 1, width * 2 + 1, width * 2, width * 2 - 1, width * 3 },
  };

  for (size_t i = 0; i < LS_ARRAYSIZE(ret.values); i++)
    ret.values[i] = lvl.grid[currentIdx + lut[a.look_at_dir][i]];

  // hidden flags
  if (ret.values[vcp_nearLeft] & tf_Collidable)
    ret.values[vcp_midLeft] = tf_Hidden;

  if (ret.values[vcp_nearCenter] & tf_Collidable)
  {
    ret.values[vcp_midCenter] = tf_Hidden;
    ret.values[vcp_farCenter] = tf_Hidden;
  }
  else if (ret.values[vcp_midCenter] & tf_Collidable)
  {
    ret.values[vcp_farCenter] = tf_Hidden;
  }

  if (ret.values[vcp_nearRight] & tf_Collidable)
    ret.values[vcp_midRight] = tf_Hidden;

  // TODO: other actor flag

  return ret;
}

void printEmpty()
{
  print("         ");
}

void printValue(const uint8_t val)
{
  tileFlag_print(val);
  print(' ');
  //print(FU(Bin, Min(8), Fill0)(val), ' ');
  //print(FU(Min(8))(val), ' ');
}

void viewCone_print(const viewCone &v, const actor &actor)
{
  print("VIEWCONE from pos ", actor.pos, " with look direction: ", lookDirection_name(actor.look_at_dir), '\n');

  printEmpty();             printValue(v[vcp_nearLeft]);    printValue(v[vcp_midLeft]);    print('\n');
  printValue(v[vcp_self]);  printValue(v[vcp_nearCenter]);  printValue(v[vcp_midCenter]);  printValue(v[vcp_farCenter]);  print('\n');
  printEmpty();             printValue(v[vcp_nearRight]);   printValue(v[vcp_midRight]);   print('\n');
}

///////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
  requires (std::is_integral_v<T> && (sizeof(T) < sizeof(int64_t) || std::is_same_v<T, int64_t>))
inline void modify_with_clamp(T &value, const int64_t diff, const T min = lsMinValue<T>(), const T max = lsMaxValue<T>())
{
  const int64_t val = (int64_t)value + diff;
  value = (T)lsClamp<int64_t>(val, min, max);
}

void actor_act(actor *pActor, level *pLevel, const viewCone &cone, const actorAction action)
{
  switch (action)
  {
  case aa_Move:
    actor_move(pActor, *pLevel);
    break;

  case aa_Move2:
    actor_moveTwo(pActor, *pLevel);
    break;

  case aa_TurnLeft:
    actor_turnLeft(pActor);
    break;

  case aa_TurnRight:
    actor_turnRight(pActor);
    break;

  case aa_Eat:
    actor_eat(pActor, pLevel, cone);
    break;

  default:
    lsFail(); // not implemented.
    break;
  }
}

void actor_updateStats(actor *pActor, const viewCone &cone)
{
  // Always update view cone first!

  static constexpr int64_t _idleEnergyCost = 3;
  static constexpr int64_t _underwaterAirCost = 5;
  static constexpr int64_t _airGain = 3;

  if (!pActor->stats[as_Energy] || !pActor->stats[as_Air])
  {
    pActor->alive = false;
    return;
  }

  // Remove energy
  modify_with_clamp(pActor->stats[as_Energy], -_idleEnergyCost);

  // Check underwater
  if (cone[vcp_self] & tf_Underwater)
    modify_with_clamp(pActor->stats[as_Air], -_underwaterAirCost);
  else
    modify_with_clamp(pActor->stats[as_Air], _airGain);
}

void actor_move(actor *pActor, const level &lvl)
{
  static constexpr size_t _movementEnergyCost = 10;

  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  //lsAssert(!(lvl.grid[pActor->pos.y * level::width + pActor->pos.x] & tf_Collidable));

  static constexpr vec2i8 lut[_lookDirection_Count] = { vec2i8(-1, 0), vec2i8(0, -1), vec2i8(1, 0), vec2i8(0, -1) };

  if (pActor->stats[as_Energy] >= _movementEnergyCost)
  {
    const vec2u16 newPos = vec2u16(pActor->pos.x + lut[pActor->look_at_dir].x, pActor->pos.y + lut[pActor->look_at_dir].y);

    if (!(lvl.grid[newPos.y * level::width + newPos.x] & tf_Collidable) && newPos.x >= level::wallThickness && newPos.x < (level::width - level::wallThickness) && newPos.y >= level::wallThickness && newPos.y < (level::height - level::wallThickness))
    {
      pActor->pos = newPos;
      pActor->stats[as_Energy] -= _movementEnergyCost;
    }
  }
}

void actor_moveTwo(actor *pActor, const level &lvl)
{
  static constexpr size_t _doubleMovementEnergyCost = 17;

  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  //lsAssert(!(lvl.grid[pActor->pos.y * level::width + pActor->pos.x] & tf_Collidable));

  static constexpr vec2i8 lut[_lookDirection_Count] = { vec2i8(-1, 0), vec2i8(0, -1), vec2i8(1, 0), vec2i8(0, -1) };

  if (pActor->stats[as_Energy] >= _doubleMovementEnergyCost)
  {
    const vec2u16 newPos = vec2u16(pActor->pos.x + 2 * lut[pActor->look_at_dir].x, pActor->pos.y + 2 * lut[pActor->look_at_dir].y);
    const size_t nearIdx = (pActor->pos.y + lut[pActor->look_at_dir].y) * level::width + (pActor->pos.x + lut[pActor->look_at_dir].x);

    if (!(lvl.grid[newPos.y * level::width + newPos.x] & tf_Collidable) && !(lvl.grid[nearIdx] & tf_Collidable) && newPos.x >= level::wallThickness && newPos.x < (level::width - level::wallThickness) && newPos.y >= level::wallThickness && newPos.y < (level::height - level::wallThickness))
    {
      pActor->pos = newPos;
      pActor->stats[as_Energy] -= _doubleMovementEnergyCost;
    }
  }
}

void actor_turnLeft(actor *pActor)
{
  static constexpr int64_t _turnEnergy = 2;

  if (pActor->stats[as_Energy] >= _turnEnergy)
  {
    pActor->stats[as_Energy] -= _turnEnergy;
    pActor->look_at_dir = pActor->look_at_dir == ld_left ? ld_down : (lookDirection)(pActor->look_at_dir - 1);
    lsAssert(pActor->look_at_dir < _lookDirection_Count);
  }
}

void actor_turnRight(actor *pActor)
{
  static constexpr int64_t _turnEnergy = 2;

  if (pActor->stats[as_Energy] >= _turnEnergy)
  {
    pActor->stats[as_Energy] -= _turnEnergy;
    pActor->look_at_dir = pActor->look_at_dir == ld_down ? ld_left : (lookDirection)(pActor->look_at_dir + 1);
    lsAssert(pActor->look_at_dir < _lookDirection_Count);
  }
}

void actor_eat(actor *pActor, level *pLvl, const viewCone &cone)
{
  // View cone must be updated **before** calling this!

  static constexpr int64_t _FoodValue = 2;
  static constexpr int64_t _FoodEnergyValue = 5;
  // TODO different values for different food;

  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);

  const size_t currentIdx = pActor->pos.y * level::width + pActor->pos.x;

  if (cone[vcp_self] & tf_Protein)
  {
    modify_with_clamp(pActor->stats[as_Protein], _FoodValue);
    modify_with_clamp(pActor->stats[as_Energy], _FoodEnergyValue);
    pLvl->grid[currentIdx] &= !tf_Protein;
  }
  if (cone[vcp_self] & tf_Sugar)
  {
    modify_with_clamp(pActor->stats[as_Sugar], _FoodValue);
    modify_with_clamp(pActor->stats[as_Energy], _FoodEnergyValue);
    pLvl->grid[currentIdx] &= !tf_Sugar;
  }
  if (cone[vcp_self] & tf_Vitamin)
  {
    modify_with_clamp(pActor->stats[as_Vitamin], _FoodValue);
    modify_with_clamp(pActor->stats[as_Energy], _FoodEnergyValue);
    pLvl->grid[currentIdx] &= !tf_Vitamin;
  }
  if (cone[vcp_self] & tf_Fat)
  {
    modify_with_clamp(pActor->stats[as_Fat], _FoodValue);
    modify_with_clamp(pActor->stats[as_Energy], _FoodEnergyValue);
    pLvl->grid[currentIdx] &= !tf_Vitamin;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////

struct nn_config // TODO!
{
  using mutator = mutator_naive;
  using crossbreeder = crossbreeder_naive;

  static constexpr size_t survivingGenes = 8;
  static constexpr size_t newGenesPerGeneration = 8;
};

template <typename crossbreeder>
void crossbreed(actor &val, const actor parentA, const actor parentB, const crossbreeder &c)
{
  for (size_t i = 0; i < LS_ARRAYSIZE(val.brain.data); i++)
    crossbreeder_eval(c, val.brain.data[i], parentA.brain.data[i], parentB.brain.data[i]);
}

template <typename mutator>
void mutate(actor &target, const mutator &m)
{
  for (size_t i = 0; i < LS_ARRAYSIZE(target.brain.data); i++)
    mutator_eval(m, target.brain.data[i], lsMinValue<uint8_t>(), lsMaxValue<uint8_t>());
}

// TODO: Eval Funcs... -> Give score
