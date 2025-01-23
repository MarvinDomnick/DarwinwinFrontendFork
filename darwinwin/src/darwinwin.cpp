#include "darwinwin.h"
#include "io.h"
#include "level_generator.h"
#include "evolution.h"
#include "local_list.h"

#include <time.h>
#include <filesystem>

//////////////////////////////////////////////////////////////////////////

level _CurrentLevel;
volatile bool _DoTraining = false;
volatile bool _TrainingRunning = false;

//////////////////////////////////////////////////////////////////////////

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

  for (size_t i = 0; i < level::width; i++)
  {
    pLevel->grid[i] = tf_Collidable;
    pLevel->grid[i + level::width] = tf_Collidable;
    pLevel->grid[i + level::width * 2] = tf_Collidable;

    pLevel->grid[i + level::width * level::height - 3 * level::width] = tf_Collidable;
    pLevel->grid[i + level::width * level::height - 2 * level::width] = tf_Collidable;
    pLevel->grid[i + level::width * level::height - 1 * level::width] = tf_Collidable;
  }

  for (size_t i = 0; i < level::height; i++)
  {
    pLevel->grid[i * level::width] = tf_Collidable;
    pLevel->grid[i * level::width + 1] = tf_Collidable;
    pLevel->grid[i * level::width + 2] = tf_Collidable;

    pLevel->grid[i * level::width + level::width - 1] = tf_Collidable;
    pLevel->grid[i * level::width + level::width - 2] = tf_Collidable;
    pLevel->grid[i * level::width + level::width - 3] = tf_Collidable;
  }
}

void printEmptyTile()
{
  lsSetConsoleColor(lsCC_DarkGray, lsCC_Black);
  print("        |");
  lsResetConsoleColor();
}

void printTile(const tileFlag val)
{
  const char lut[9] = "UPSVFCOH";
  const lsConsoleColor fg[8] = { lsCC_BrightBlue, lsCC_BrightMagenta, lsCC_White, lsCC_BrightGreen, lsCC_BrightYellow, lsCC_BrightGray, lsCC_BrightCyan, lsCC_BrightRed };

  for (size_t i = 0, mask = 1; i < 8; mask <<= 1, i++)
  {
    lsSetConsoleColor(fg[i], lsCC_Black);
    print((val & mask) ? lut[i] : ' ');
  }

  lsSetConsoleColor(lsCC_DarkGray, lsCC_Black);
  print('|');
  lsResetConsoleColor();
}

void level_print(const level &level)
{
  print("Level \n");

  lsSetConsoleColor(lsCC_DarkGray, lsCC_Black);
  for (size_t x = 0; x < level::width; x++)
    print("        |");

  print('\n');
  lsResetConsoleColor();

  for (size_t y = 0; y < level::height; y++)
  {
    for (size_t x = 0; x < level::width; x++)
      printTile(level.grid[y * level::width + x]);

    print('\n');

    lsSetConsoleColor(lsCC_DarkGray, lsCC_Black);
    for (size_t x = 0; x < level::width; x++)
      print("--------|");

    print('\n');
    lsResetConsoleColor();
  }

  print('\n');
}

//////////////////////////////////////////////////////////////////////////

void level_gen_water_level(level *pLvl)
{
  level_gen_init(pLvl, tf_Underwater);
  level_gen_random_sprinkle_replace_mask(pLvl, tf_Underwater, 0, level::total / 10);
  level_gen_grow(pLvl, 0);
  level_gen_sprinkle_grow_into_inv_mask(pLvl, tf_Underwater, tf_Underwater, level_gen_make_chance<0.5>());
  level_gen_finalize(pLvl);
}

void level_gen_water_food_level(level *pLvl)
{
  level_gen_init(pLvl, tf_Underwater);
  level_gen_random_sprinkle_replace_mask(pLvl, tf_Underwater, 0, level::total / 10);
  level_gen_grow(pLvl, 0);
  level_gen_random_sprinkle_replace_inv_mask(pLvl, tf_Underwater, tf_Vitamin | tf_Underwater, level::total / 10);
  level_gen_random_sprinkle_replace(pLvl, tf_Vitamin | tf_Underwater, tf_Vitamin | tf_Underwater | tf_Fat, level::total / 3); // UVF looks sus
  level_gen_sprinkle_grow_into_mask(pLvl, tf_Underwater | tf_Vitamin, tf_Underwater, level_gen_make_chance<0.75>());
  level_gen_sprinkle_grow_into_inv_mask(pLvl, tf_Underwater, tf_Underwater, level_gen_make_chance<0.5>());
  level_gen_random_sprinkle_replace_inv_mask(pLvl, tf_Underwater, tf_Protein, level::total / 10);
  level_gen_finalize(pLvl);
}

//////////////////////////////////////////////////////////////////////////

void level_generateDefault(level *pLvl)
{
  level_gen_water_food_level(pLvl);
}

//////////////////////////////////////////////////////////////////////////

bool level_performStep(level &lvl, actor *pActors, const size_t actorCount)
{
  // TODO: optional level internal step. (grow plants, etc.)

  bool anyAlive = false;

  for (size_t i = 0; i < actorCount; i++)
  {
    if (!pActors[i].stats[as_Energy])
      continue;

    anyAlive = true;

    const viewCone cone = viewCone_get(lvl, pActors[i]);
    actor_updateStats(&pActors[i], cone);

    decltype(actor::brain)::io_buffer_t ioBuffer;

    for (size_t j = 0; j < LS_ARRAYSIZE(cone.values); j++)
      for (size_t k = 0, bit = 1; k < 8; k++, bit <<= 1)
        ioBuffer[j * 8 + k] = (int8_t)(cone[(viewConePosition)j] & bit);

    neural_net_buffer_prepare(ioBuffer, (LS_ARRAYSIZE(cone.values) * 8) / ioBuffer.block_size);

    for (size_t j = 0; j < _actorStats_Count; j++)
      ioBuffer[LS_ARRAYSIZE(cone.values) * 8 + j] = (int8_t)((int64_t)pActors[i].stats[j] - 128);

    lsMemcpy(&ioBuffer[pActors[i].brain.first_layer_count - LS_ARRAYSIZE(pActors[i].previous_feedback_output)], pActors[i].previous_feedback_output, LS_ARRAYSIZE(pActors[i].previous_feedback_output));

    neural_net_eval(pActors[i].brain, ioBuffer);

    size_t actionValueCount = 0;

    static_assert(_actorAction_Count <= LS_ARRAYSIZE(ioBuffer.data));

    for (size_t j = 0; j < _actorAction_Count; j++)
      actionValueCount += ioBuffer[j];

    size_t bestActionIndex = 0;

    if (actionValueCount > 0)
    {
      const size_t rand = lsGetRand() % actionValueCount;

      for (size_t actionIndex = 0; actionIndex < _actorAction_Count; actionIndex++)
      {
        const int16_t val = ioBuffer[actionIndex];

        if (val < rand)
        {
          bestActionIndex = actionIndex;
          break;
        }

        actionValueCount -= val;
      }

      pActors[i].last_action = (actorAction)bestActionIndex;
      actor_act(&pActors[i], &lvl, cone, pActors[i].last_action);
    }

    lsMemcpy(pActors[i].previous_feedback_output, &ioBuffer[pActors[i].brain.last_layer_count - LS_ARRAYSIZE(pActors[i].previous_feedback_output)], LS_ARRAYSIZE(pActors[i].previous_feedback_output));
  }

  return anyAlive;
}

//////////////////////////////////////////////////////////////////////////

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
    ret.values[i] = lvl.grid[currentIdx + lut[a.look_dir][i]];

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

void viewCone_print(const viewCone &v, const actor &actor)
{
  print("VIEWCONE from pos ", actor.pos, " with look direction: ", lookDirection_name(actor.look_dir), '\n');

  printEmptyTile();        printTile(v[vcp_nearLeft]);    printTile(v[vcp_midLeft]);    print('\n');
  printTile(v[vcp_self]);  printTile(v[vcp_nearCenter]);  printTile(v[vcp_midCenter]);  printTile(v[vcp_farCenter]);  print('\n');
  printEmptyTile();        printTile(v[vcp_nearRight]);   printTile(v[vcp_midRight]);   print('\n');
}

//////////////////////////////////////////////////////////////////////////

void actor_move(actor *pActor, const level &lvl);
void actor_moveTwo(actor *pActor, const level &lvl);
void actor_turnLeft(actor *pActor);
void actor_turnRight(actor *pActor);
void actor_eat(actor *pActor, level *pLvl, const viewCone &cone);
void actor_moveDiagonalLeft(actor *pActor, const level lvl);
void actor_moveDiagonalRight(actor *pActor, const level lvl);

//////////////////////////////////////////////////////////////////////////

template <typename T>
  requires (std::is_integral_v<T> && (sizeof(T) < sizeof(int64_t) || std::is_same_v<T, int64_t>))
inline T modify_with_clamp(T &value, const int64_t diff, const T min = lsMinValue<T>(), const T max = lsMaxValue<T>())
{
  const int64_t val = (int64_t)value + diff;
  const T prevVal = value;
  value = (T)lsClamp<int64_t>(val, min, max);
  return value - prevVal;
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

  case aa_Wait:
    break;

  case aa_DiagonalMoveLeft:
    actor_moveDiagonalLeft(pActor, *pLevel);
    break;

  case aa_DiagonalMoveRight:
    actor_moveDiagonalRight(pActor, *pLevel);
    break;

  default:
    lsFail(); // not implemented.
    break;
  }
}

void actor_initStats(actor *pActor)
{
  for (size_t i = 0; i < _actorStats_Count; i++)
    pActor->stats[i] = 32;

  pActor->stats[as_Air] = 127;
  pActor->stats[as_Energy] = 127;
}

void actor_updateStats(actor *pActor, const viewCone &cone)
{
  constexpr int64_t IdleEnergyCost = 2;

  // Remove Idle Energy
  modify_with_clamp(pActor->stats[as_Energy], -IdleEnergyCost);

  // Check air
  constexpr int64_t UnderwaterAirCost = 5;
  constexpr int64_t SurfaceAirAmount = 3;
  constexpr int64_t NoAirEnergyCost = 8;

  if (cone[vcp_self] & tf_Underwater)
    modify_with_clamp(pActor->stats[as_Air], -UnderwaterAirCost);
  else
    modify_with_clamp(pActor->stats[as_Air], SurfaceAirAmount);

  if (!pActor->stats[as_Air])
    modify_with_clamp(pActor->stats[as_Energy], -NoAirEnergyCost);

  // Digest
  constexpr int64_t FoodEnergyAmount = 5;
  constexpr int64_t FoodDigestionAmount = 1;

  size_t count = 0;

  for (size_t i = _actorStats_FoodBegin; i <= _actorStats_FoodEnd; i++)
  {
    if (pActor->stats[i])
    {
      modify_with_clamp(pActor->stats[i], -FoodDigestionAmount);
      count++;
    }
  }

  modify_with_clamp(pActor->stats[as_Energy], count * FoodEnergyAmount);
}

//////////////////////////////////////////////////////////////////////////

constexpr int64_t CollideEnergyCost = 4;

void actor_move(actor *pActor, const level &lvl)
{
  constexpr int64_t MovementEnergyCost = 10;
  constexpr vec2i16 lut[_lookDirection_Count] = { vec2i16(-1, 0), vec2i16(0, -1), vec2i16(1, 0), vec2i16(0, 1) };

  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  lsAssert(!(lvl.grid[pActor->pos.y * level::width + pActor->pos.x] & tf_Collidable));

  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -MovementEnergyCost);

  if (oldEnergy < MovementEnergyCost)
    return;

  const vec2u16 newPos = vec2u16(vec2i16(pActor->pos) + lut[pActor->look_dir]);
  const size_t newIdx = newPos.y * level::width + newPos.x;

  if (!!(lvl.grid[newIdx] & tf_Collidable))
  {
    modify_with_clamp(pActor->stats[as_Energy], -CollideEnergyCost);
    return;
  }

  lsAssert(!(lvl.grid[newIdx] & tf_Collidable));
  lsAssert(newPos.x < level::width - level::wallThickness && newPos.y < level::height - level::wallThickness && newPos.x >= level::wallThickness && newPos.y >= level::wallThickness);

  pActor->pos = newPos;
}

void actor_moveTwo(actor *pActor, const level &lvl)
{
  constexpr int64_t DoubleMovementEnergyCost = 30;
  constexpr vec2i16 LutPosDouble[_lookDirection_Count] = { vec2i16(-2, 0), vec2i16(0, -2), vec2i16(2, 0), vec2i16(0, 2) };
  constexpr int8_t LutIdxSingle[_lookDirection_Count] = { -1, -(int64_t)level::width, 1, level::width };

  const size_t currentIdx = pActor->pos.y * level::width + pActor->pos.x;
  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  lsAssert(!(lvl.grid[currentIdx] & tf_Collidable));

  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -DoubleMovementEnergyCost);

  if (oldEnergy < DoubleMovementEnergyCost)
    return;

  const size_t nearIdx = currentIdx + LutIdxSingle[pActor->look_dir];
  const size_t newPosIdx = currentIdx + 2 * LutIdxSingle[pActor->look_dir];

  if (!!(lvl.grid[newPosIdx] & tf_Collidable) || !!(lvl.grid[nearIdx] & tf_Collidable))
  {
    modify_with_clamp(pActor->stats[as_Energy], -CollideEnergyCost);
    return;
  }

  const vec2u16 newPos = (vec2u16)((vec2i16)(pActor->pos) + LutPosDouble[pActor->look_dir]);

  lsAssert(newPosIdx == newPos.y * level::width + newPos.x);
  lsAssert(!(lvl.grid[newPosIdx] & tf_Collidable));
  lsAssert(newPos.x < level::width - level::wallThickness && newPos.y < level::height - level::wallThickness && newPos.x >= level::wallThickness && newPos.y >= level::wallThickness);

  pActor->pos = newPos;
}

constexpr int64_t TurnEnergy = 2;

void actor_turnLeft(actor *pActor)
{
  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -TurnEnergy);

  if (oldEnergy < TurnEnergy)
    return;

  pActor->look_dir = pActor->look_dir == ld_left ? ld_down : (lookDirection)(pActor->look_dir - 1);
  lsAssert(pActor->look_dir < _lookDirection_Count);
}

void actor_turnRight(actor *pActor)
{
  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -TurnEnergy);

  if (oldEnergy < TurnEnergy)
    return;

  pActor->look_dir = pActor->look_dir == ld_down ? ld_left : (lookDirection)(pActor->look_dir + 1);
  lsAssert(pActor->look_dir < _lookDirection_Count);
}

void actor_eat(actor *pActor, level *pLvl, const viewCone &cone)
{
  static constexpr int64_t EatEnergyCost = 3;
  static constexpr int64_t FoodAmount = 2;
  static constexpr uint8_t StomachCapacity = 255;

  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);

  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -EatEnergyCost);

  if (oldEnergy < TurnEnergy)
    return;

  size_t stomachFoodCount = 0;

  for (size_t i = _actorStats_FoodBegin; i <= _actorStats_FoodEnd; i++)
    stomachFoodCount += pActor->stats[i];

  lsAssert(stomachFoodCount <= StomachCapacity);

  for (size_t i = _actorStats_FoodBegin; i <= _actorStats_FoodEnd; i++)
  {
    if (cone[vcp_self] & (1ULL << i))
    {
      stomachFoodCount += modify_with_clamp(pActor->stats[i], FoodAmount, lsMinValue<uint8_t>(), (uint8_t)((StomachCapacity - stomachFoodCount) + pActor->stats[i]));
      pLvl->grid[pActor->pos.y * level::width + pActor->pos.x] &= ~(1ULL << i);
    }
  }
}

static constexpr int64_t MoveDiagonalCost = 16;

void actor_moveDiagonalLeft(actor *pActor, const level lvl)
{
  const size_t currentIdx = pActor->pos.y * level::width + pActor->pos.x;
  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  lsAssert(!(lvl.grid[currentIdx] & tf_Collidable));

  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -MoveDiagonalCost);

  if (oldEnergy < MoveDiagonalCost)
    return;

  constexpr vec2i16 lut[_lookDirection_Count] = { vec2i16(-1, 1), vec2i16(-1, -1), vec2i16(1, -1), vec2i16(1, 1) };

  const vec2u16 newPos = (vec2u16)((vec2i16)(pActor->pos) + lut[pActor->look_dir]);
  const size_t newIdx = newPos.y * level::width + newPos.x;

  if (!!(lvl.grid[newIdx] & tf_Collidable))
  {
    modify_with_clamp(pActor->stats[as_Energy], -CollideEnergyCost);
    return;
  }

  lsAssert(!(lvl.grid[newIdx] & tf_Collidable));
  lsAssert(newPos.x < level::width - level::wallThickness && newPos.y < level::height - level::wallThickness && newPos.x >= level::wallThickness && newPos.y >= level::wallThickness);

  pActor->pos = newPos;
}

void actor_moveDiagonalRight(actor *pActor, const level lvl)
{
  const size_t currentIdx = pActor->pos.y * level::width + pActor->pos.x;
  lsAssert(pActor->pos.x < level::width && pActor->pos.y < level::height);
  lsAssert(!(lvl.grid[currentIdx] & tf_Collidable));

  const size_t oldEnergy = pActor->stats[as_Energy];
  modify_with_clamp(pActor->stats[as_Energy], -MoveDiagonalCost);

  if (oldEnergy < MoveDiagonalCost)
    return;

  constexpr vec2i16 lut[_lookDirection_Count] = { vec2i16(-1, -1), vec2i16(1, -1), vec2i16(1, 1), vec2i16(-1, 1) };

  const vec2u16 newPos = (vec2u16)((vec2i16)(pActor->pos) + lut[pActor->look_dir]);
  const size_t newIdx = newPos.y * level::width + newPos.x;

  if (!!(lvl.grid[newIdx] & tf_Collidable))
  {
    modify_with_clamp(pActor->stats[as_Energy], -CollideEnergyCost);
    return;
  }

  lsAssert(!(lvl.grid[newIdx] & tf_Collidable));
  lsAssert(newPos.x < level::width - level::wallThickness && newPos.y < level::height - level::wallThickness && newPos.x >= level::wallThickness && newPos.y >= level::wallThickness);

  pActor->pos = newPos;
}

//////////////////////////////////////////////////////////////////////////

lsResult actor_saveBrain(const char *dir, const actor &actr)
{
  lsResult result = lsR_Success;

  const uint64_t now = (uint64_t)time(nullptr);
  char filename[256];
  sformat_to(filename, LS_ARRAYSIZE(filename), dir, "/", now, ".brain");

  print("Saving brain to file: '", filename, '\n');

  {
    cached_file_byte_stream_writer<> write_stream;
    LS_ERROR_CHECK(write_byte_stream_init(write_stream, filename));
    value_writer<decltype(write_stream)> writer;
    LS_ERROR_CHECK(value_writer_init(writer, &write_stream));

    LS_ERROR_CHECK(neural_net_write(actr.brain, writer));
    LS_ERROR_CHECK(write_byte_stream_flush(write_stream));
  }

epilogue:
  return result;
}

lsResult actor_loadBrainFromFile(const char *filename, actor &actr)
{
  lsResult result = lsR_Success;

  print("Loading brain from file: ", filename, '\n');

  cached_file_byte_stream_reader<> read_stream;
  value_reader<cached_file_byte_stream_reader<>> reader;
  LS_ERROR_CHECK(read_byte_stream_init(read_stream, filename));
  LS_ERROR_CHECK(value_reader_init(reader, &read_stream));

  LS_ERROR_CHECK(neural_net_read(actr.brain, reader));
  read_byte_stream_destroy(read_stream);

epilogue:
  return result;
}

lsResult actor_loadNewestBrain(const char *dir, actor &actr)
{
  lsResult result = lsR_Success;

  const std::filesystem::path path(dir);

  int64_t bestTime = -1;
  std::string best;

  for (const std::filesystem::directory_entry &dir_entry : std::filesystem::directory_iterator(dir))
  {
    if (dir_entry.is_regular_file())
    {
      const std::filesystem::file_time_type &timestamp = dir_entry.last_write_time();
      const int64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

      if (bestTime < timeMs)
      {
        bestTime = timeMs;
        best = dir_entry.path().filename().string();
      }
    }
  }

  LS_ERROR_IF(best.empty(), lsR_ResourceNotFound);
  lsAssert(bestTime >= 0);
  char filename[256];
  sformat_to(filename, LS_ARRAYSIZE(filename), dir, '/', best.c_str());
  LS_ERROR_CHECK(actor_loadBrainFromFile(filename, actr));

epilogue:
  return result;
}

// load specific brain: list and then select in console

//////////////////////////////////////////////////////////////////////////

struct starter_random_config
{
  using mutator = mutator_random;
  using crossbreeder = crossbreeder_naive;

  static constexpr size_t survivingGenes = 16;
  static constexpr size_t newGenesPerGeneration = 3 * 2 * 5 * 8;
};

struct starter_random_config_independent : starter_random_config
{
  static constexpr size_t survivingGenes = 4;
  static constexpr size_t newGenesPerGeneration = 16;
};

template <typename crossbreeder>
void crossbreed(actor &val, const actor parentA, const actor parentB, const crossbreeder &c)
{
  val.look_dir = parentA.look_dir;
  val.pos = parentA.pos;
  lsMemcpy(val.stats, parentA.stats, LS_ARRAYSIZE(val.stats));

  crossbreeder_eval(c, val.brain.values, LS_ARRAYSIZE(val.brain.values), parentA.brain.values, parentB.brain.values);
}

template <typename mutator>
void mutate(actor &target, const mutator &m)
{
  mutator_eval(m, &target.brain.values[0], LS_ARRAYSIZE(target.brain.values), (int16_t)lsMinValue<int8_t>(), (int16_t)lsMaxValue<int8_t>());
}

// TODO: Eval Funcs... -> Give scores
constexpr size_t EvaluatingCycles = 1000;

size_t evaluate_actor(const actor &in)
{
  constexpr size_t foodSprinkleMask = (1ULL << 5) - 1;

  actor actr = in;
  level lvl = _CurrentLevel;
  size_t score = 0;

  for (size_t i = 0; i < EvaluatingCycles; i++)
  {
    const uint8_t foodCapacityBefore = actr.stomach_remaining_capacity;

    if ((i & foodSprinkleMask) == 0)
    {
      // TODO: Sprinkle Foods.
    }

    if (!level_performStep(lvl, &actr, 1))
      break;

    score++;
    score += ((uint8_t)(foodCapacityBefore > actr.stomach_remaining_capacity)) * 3;
  }

  return score;
}

size_t evaluate_null(const actor &)
{
  return 0;
}

constexpr size_t generationsPerLevel = 128;

lsResult train_loop(thread_pool *pThreadPool, const char *dir)
{
  lsResult result = lsR_Success;

  constexpr bool trainSynchronously = true;

  {
    actor actr;
    actor_loadNewestBrain(dir, actr);

    uint64_t rand = lsGetRand();
    actr.pos = vec2u16((rand & 0xFFFF) % (level::width - level::wallThickness * 2), ((rand >> 16) & 0xFFFF) % (level::height - level::wallThickness * 2));
    actr.pos += vec2u16(level::wallThickness);
    actr.look_dir = (lookDirection)((rand >> 32) % _lookDirection_Count);

    actor_initStats(&actr);

    evolution<actor, starter_random_config> evl;
    evolution_init(evl, actr, evaluate_null); // because no level is generated yet!

    size_t levelIndex = 0;

    while (_DoTraining)
    {
      level_generateDefault(&_CurrentLevel);
      size_t maxRetries = 32;

      do
      {
        rand = lsGetRand();
        actr.pos = vec2u16((rand & 0xFFFF) % (level::width - level::wallThickness * 2), ((rand >> 16) & 0xFFFF) % (level::height - level::wallThickness * 2));
        actr.pos += vec2u16(level::wallThickness);
        actr.look_dir = (lookDirection)((rand >> 32) % _lookDirection_Count);
        maxRetries--;
      } while ((_CurrentLevel.grid[actr.pos.x + actr.pos.y * level::width] & tf_Collidable) && maxRetries);

      evolution_for_each(evl, [&](actor &a) { a.pos = actr.pos; a.look_dir = actr.look_dir; });

      if (maxRetries == 0)
      {
        print_error_line("Failed to find non-collidable position in lvl.");
        continue;
      }

      evolution_reevaluate(evl, evaluate_actor);

      const actor *pBest = nullptr;
      size_t score, bestScore = 0;

      for (size_t i = 0; i < generationsPerLevel && _DoTraining; i++)
      {
        if constexpr (trainSynchronously)
          evolution_generation(evl, evaluate_actor);
        else
          evolution_generation(evl, evaluate_actor, pThreadPool);

        evolution_get_best(evl, &pBest, score);

        if (score > bestScore)
        {
          print_log_line("New Best: Level ", levelIndex, ", Generation ", i, ": ", score);
          bestScore = score;
        }
      }

      LS_ERROR_CHECK(actor_saveBrain(dir, *pBest));
      levelIndex++;
    }
  }

epilogue:
  _TrainingRunning = false;
  return result;
}

lsResult train_loopIndependentEvolution(thread_pool *pThreadPool, const char *dir)
{
  lsResult result = lsR_Success;

  using config = starter_random_config_independent;
  using evl_type = evolution<actor, config>;
  small_list<evl_type> evolutions;

  struct actor_ref
  {
    size_t score, evolution_idx, idx;
    actor_ref() = default; // actor_ref': no appropriate default constructor available -> yes?
    actor_ref(const size_t score, const size_t evolution_idx, const size_t idx) : score(score), evolution_idx(evolution_idx), idx(idx) {}
    bool operator > (const actor_ref &other) const { return score < other.score; }
    bool operator < (const actor_ref &other) const { return score > other.score; }
  };

  small_list<actor_ref> best_actor_refs;
  actor best_actors[config::survivingGenes];

  actor actr;
  actor_loadNewestBrain(dir, actr);

  uint64_t rand = lsGetRand();
  actr.pos = vec2u16((rand & 0xFFFF) % (level::width - level::wallThickness * 2), ((rand >> 16) & 0xFFFF) % (level::height - level::wallThickness * 2));
  actr.pos += vec2u16(level::wallThickness);
  actr.look_dir = (lookDirection)((rand >> 32) % _lookDirection_Count);

  actor_initStats(&actr);
  size_t trainingCycle = 0;
  const size_t geneGenerationCount = thread_pool_thread_count(pThreadPool) * config::newGenesPerGeneration * generationsPerLevel;

  print("Starting Training: ", thread_pool_thread_count(pThreadPool), " Threads x ", config::newGenesPerGeneration, " Genes x ", generationsPerLevel, " Generations / Level x ", EvaluatingCycles, " Evaluating Cycles Max = ", FU(Group)(thread_pool_thread_count(pThreadPool) * config::newGenesPerGeneration * generationsPerLevel * EvaluatingCycles), '\n');

  for (size_t i = 0; i < thread_pool_thread_count(pThreadPool); i++)
  {
    evl_type evl;
    LS_ERROR_CHECK(evolution_init_empty(evl));

    evolution_add_unevaluated_target(evl, std::move(actr));

    LS_ERROR_CHECK(list_add(&evolutions, std::move(evl)));
  }

  while (_DoTraining)
  {
    list_clear(&best_actor_refs);

    level_generateDefault(&_CurrentLevel);
    size_t maxRetries = 32;

    do
    {
      rand = lsGetRand();
      actr.pos = vec2u16((rand & 0xFFFF) % (level::width - level::wallThickness * 2), ((rand >> 16) & 0xFFFF) % (level::height - level::wallThickness * 2));
      actr.pos += vec2u16(level::wallThickness);
      actr.look_dir = (lookDirection)((rand >> 32) % _lookDirection_Count);
      maxRetries--;
    } while ((_CurrentLevel.grid[actr.pos.x + actr.pos.y * level::width] & tf_Collidable) && maxRetries);

    for (auto &evol : evolutions)
      evolution_for_each(evol, [&](actor &a) { a.pos = actr.pos; a.look_dir = actr.look_dir; });

    if (maxRetries == 0)
    {
      print_error_line("Failed to find non-collidable position in lvl.");
      continue;
    }

    for (auto &evol : evolutions)
      evolution_reevaluate(evol, evaluate_actor);

    const int64_t startNs = lsGetCurrentTimeNs();

    for (size_t i = 0; i < evolutions.count; i++)
    {
      evl_type *pEvolution = &evolutions[i];

      std::function<void()> async_func = [=]()
        {
          for (size_t j = 0; j < generationsPerLevel && _DoTraining; j++)
            evolution_generation(*pEvolution, evaluate_actor);
        };

      thread_pool_add(pThreadPool, async_func);
    }

    thread_pool_await(pThreadPool);

    const int64_t endNs = lsGetCurrentTimeNs();

    // extract actor refs w/ score
    size_t index = 0;
    for (auto &evol : evolutions)
    {
      for (size_t j = 0; j < evolution_get_count(evol); j++)
      {
        size_t idx;
        size_t score;
        evolution_get_at(evol, j, idx, score);
        LS_ERROR_CHECK(list_add(&best_actor_refs, actor_ref(score, index, idx)));
      }

      index++;
    }

    // sort
    list_sort(best_actor_refs);

    // extract best actors to best_actors
    for (size_t i = 0; i < LS_ARRAYSIZE(best_actors); i++)
      best_actors[i] = std::move(pool_get(evolutions[best_actor_refs[i].evolution_idx].genes, best_actor_refs[i].idx)->t);

    for (auto &evol : evolutions)
    {
      // clear evolutions
      evolution_clear(evol);

      // insert best actors to evolutions
      for (size_t i = 0; i < LS_ARRAYSIZE(best_actors); i++)
        evolution_add_unevaluated_target(evol, best_actors[i]);
    }

    const actor_ref *pBestRef = list_get(&best_actor_refs, 0);
    print_log_line("Current Best: Training Cycle: ", trainingCycle, " w/ score: ", pBestRef->score, " (", FD(Group, Frac(3))(geneGenerationCount / ((endNs - startNs) * 1e-9)), " Generations/s)");
    LS_ERROR_CHECK(actor_saveBrain(dir, best_actors[0]));

    trainingCycle++;
  }

epilogue:
  _TrainingRunning = false;
  return result;
}

