#include "dumbdisplay.h"

#if defined(BLUETOOTH)
  #include "esp32dumbdisplay.h"
  DumbDisplay dumbdisplay(new DDBluetoothSerialIO(BLUETOOTH));
#elif defined(WIFI_SSID)
  #include "wifidumbdisplay.h"
  DumbDisplay dumbdisplay(new DDWiFiServerIO(WIFI_SSID, WIFI_PASSWORD));
#else
  #include "dumbdisplay.h"
  DumbDisplay dumbdisplay(new DDInputOutput());
#endif


#define TILE_COUNT              4
#define SUPPORT_AI     

// experimental
//#define AI_STAGED_STAGE_COUNT   2


// experimental
// #define AI_SUGGEST_LM_COUNT     2


#define SUGGEST_MAX_DEPTH       10
#define SUGGEST_MIN_DEPTH       3

#define FB_ACCEPTABLE_AI_PROB   0.4
#define FB_SURE_AI_PROB         0.9



#ifndef SUGGEST_MAX_DEPTH
  #undef SUPPORT_AI
#endif



#ifdef SUPPORT_AI

  #include <TensorFlowLite_ESP32.h>
  #include "tensorflow/lite/micro/all_ops_resolver.h"
  #include "tensorflow/lite/micro/micro_error_reporter.h"
  #include "tensorflow/lite/micro/micro_interpreter.h"
  #include "tensorflow/lite/schema/schema_generated.h"

  // error reporter for TensorFlow Lite
  class DDTFLErrorReporter : public tflite::ErrorReporter {
  public:
    virtual int Report(const char* format, va_list args) {
      int len = strlen(format);
      char buffer[max(32, 2 * len)];  // assume 2 times format len is big enough
      sprintf(buffer, format, args);
      dumbdisplay.writeComment(buffer);
      return 0;
    }
  };


  #if defined(AI_STAGED_STAGE_COUNT) && AI_STAGED_STAGE_COUNT == 3
    #if TILE_COUNT == 4
      #include "sp_staged_model_4_3.h"
    #elif TILE_COUNT == 5
      #include "sp_staged_model_5_3.h"
    #endif
  #elif defined(AI_STAGED_STAGE_COUNT) && AI_STAGED_STAGE_COUNT == 2
    #if TILE_COUNT == 4
      #include "sp_staged_model_4_2.h"
    #elif TILE_COUNT == 5
      #include "sp_staged_model_5_2.h"
    #endif
  #elif defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT == 1
    #if TILE_COUNT == 4
      #include "sp_lm_model_4_1.h"
    #elif TILE_COUNT == 5
      #include "sp_lm_model_5_1.h"
    #endif
  #elif defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT == 15
    #if TILE_COUNT == 4
      #include "sp_lm_model_4_15.h"
    #elif TILE_COUNT == 5
      #include "sp_lm_model_5_15.h"
    #endif
  #else
    #if TILE_COUNT == 4
      #include "sp_model_4.h"
    #elif TILE_COUNT == 5
      #include "sp_model_5.h"
    #endif
  #endif
  const tflite::Model* model = ::tflite::GetModel(sp_model);

  tflite::ErrorReporter* error_reporter = new DDTFLErrorReporter();
  const int tensor_arena_size = 81 * 1024;  // guess ... the same as used by esp32cam_person (person detection)
  uint8_t* tensor_arena;
  tflite::MicroInterpreter* interpreter = NULL;
  TfLiteTensor* input;

  int coopMode = 0;

#endif  



// DDMasterResetPassiveConnectionHelper is for making "passive" connection; i.e. it can be reconnected after disconnect
DDMasterResetPassiveConnectionHelper pdd(dumbdisplay);

// the only layer for the board
GraphicalDDLayer* board;

const int BOARD_SIZE = 400;
const int TILE_SIZE = BOARD_SIZE / TILE_COUNT;
const int STARTOFF_RANDOMIZE_TILE_STEP_COUNT = 5;


// tells what tile Id (basically tile level id) is at what tile position
int boardTileIds[TILE_COUNT][TILE_COUNT];

long waitingToRestartMillis = -1;  // -1 means not waiting

int holeTileColIdx;  // -1 means board not initialize
int holeTileRowIdx;

short randomizeCanMoveFromDirs[4];
long randomizeMoveTileInMillis;
int initRandomizeTileStepCount;
int randomizeTilesStepCount;
short randomizeCanMoveFromDir;

int moveTileColIdx;
int moveTileRowIdx;
int moveTileFromDir;
int moveTileDelta;
int moveTileRefX;
int moveTileRefY;
int moveTileId;


#ifdef SUGGEST_MAX_DEPTH

  LcdDDLayer* resetButton;

  // the "selections" for suggesting moves
  SelectionDDLayer* suggestSelection;

  SelectionDDLayer* coopSelection;


  const long suggestedMoveTileInMillis= 250;
  bool suggestContinuously;
  
#endif
 
  



int checkCanMoveFromDirs(short* canMoveFromDirs, short prevCanMoveFromDir = -1) {  // prevCanMoveFromDir -1 means no previous direction
  int canCount = 0;
  bool bias = random(2) == 0;
  if (bias) {
    // bias moving hole to center
    if (holeTileColIdx > 0 && prevCanMoveFromDir != 1) {
      canMoveFromDirs[canCount++] = 0;  // 0: left
      if (holeTileColIdx == (TILE_COUNT - 1)) {
        canMoveFromDirs[canCount++] = 0;  // 0: left ... if cannot move from right, bias move from left
      }
    }
    if (holeTileColIdx < (TILE_COUNT - 1) && prevCanMoveFromDir != 0) {
      canMoveFromDirs[canCount++] = 1;  // 1: right
      if (holeTileColIdx == 0) {
        canMoveFromDirs[canCount++] = 1;  // 1: right ... if cannot move from left, bias move from right
      }
    }
    if (holeTileRowIdx > 0 && prevCanMoveFromDir != 3) {
      canMoveFromDirs[canCount++] = 2;  // 2: up
      if (holeTileRowIdx == (TILE_COUNT - 1)) {
        canMoveFromDirs[canCount++] = 2;  // 2: up ... if cannot move from down, bias move from up
      }
    }
    if (holeTileRowIdx < (TILE_COUNT - 1) && prevCanMoveFromDir != 2) {
      canMoveFromDirs[canCount++] = 3;  // 3: down
      if (holeTileRowIdx == 0) {
        canMoveFromDirs[canCount++] = 3;  // 3: down ... if cannot move from up, bias move from down
      }
    }
  } else {
    if (holeTileColIdx > 0 && prevCanMoveFromDir != 1) {
      canMoveFromDirs[canCount++] = 0;  // 0: left
    }
    if (holeTileColIdx < (TILE_COUNT - 1) && prevCanMoveFromDir != 0) {
      canMoveFromDirs[canCount++] = 1;  // 1: right
    }
    if (holeTileRowIdx > 0 && prevCanMoveFromDir != 3) {
      canMoveFromDirs[canCount++] = 2;  // 2: up
    }
    if (holeTileRowIdx < (TILE_COUNT - 1) && prevCanMoveFromDir != 2) {
      canMoveFromDirs[canCount++] = 3;  // 3: down
    }
  }
  return canCount;
}

void canMoveFromDirToFromIdxes(short canMoveFromDir, int& fromColIdx, int& fromRowIdx) {
  if (canMoveFromDir == 0) {
    fromColIdx = holeTileColIdx - 1;
    fromRowIdx = holeTileRowIdx;
  } else if (canMoveFromDir == 1) {
    fromColIdx = holeTileColIdx + 1;
    fromRowIdx = holeTileRowIdx;
  } else if (canMoveFromDir == 2) {
    fromColIdx = holeTileColIdx;
    fromRowIdx = holeTileRowIdx - 1;
  } else {
    fromColIdx = holeTileColIdx;
    fromRowIdx = holeTileRowIdx + 1;
  }
}


// show / hide the hole tile, which might not be in position
void showHideHoleTile(bool show) {
  int holeTileId = boardTileIds[holeTileRowIdx][holeTileColIdx];
  String holeTileLevelId = String(holeTileId);
  int anchorX = holeTileColIdx * TILE_SIZE;
  int anchorY = holeTileRowIdx * TILE_SIZE;
  board->switchLevel(holeTileLevelId);
  board->setLevelAnchor(anchorX, anchorY);
  board->setLevelAnchor(0, 0);
  board->levelTransparent(!show);
}



void initializeBoard() {
  dumbdisplay.log("Creating board ...");

  // export what has been draw as an image named "boardimg"
  board->exportLevelsAsImage("boardimg", true);
  
  board->clear();

  // add a "ref" level and draw the exported image "boardimg" on it (as reference)
  board->addLevel("ref", true);
  board->levelOpacity(5);
  board->drawImageFile("boardimg");
  
  for (int rowTileIdx = 0; rowTileIdx < TILE_COUNT; rowTileIdx++) {
    for (int colTileIdx = 0; colTileIdx < TILE_COUNT; colTileIdx++) {
      int tileId = colTileIdx + rowTileIdx * TILE_COUNT;

      // imageName refers to a tile of the image "boardimg"; e.g. "0!4x4@boardimg" refers to the 0th tile of a 4x4 image named "boardimg"
      String imageName = String(tileId) + "!" + String(TILE_COUNT) + "x" + String(TILE_COUNT) + "@boardimg";
      
      String tileLevelId = String(tileId);
      int x = colTileIdx * TILE_SIZE;
      int y = rowTileIdx * TILE_SIZE;

      // add a level that represents a tile ... and switch to it
      board->addLevel(tileLevelId, TILE_SIZE, TILE_SIZE, true);

      // the the tile anchor of the level to the tile position on the board
      board->setLevelAnchor(x, y);

      // set the back of the level to the tile image, with board (b:3-gray-round)
      board->setLevelBackground("", imageName, "b:3-gray-round");
      
      boardTileIds[rowTileIdx][colTileIdx] = tileId;
    }
  }

  // reorder the "ref" level to the bottom, so that it will be drawn underneath the tiles
  board->reorderLevel("ref", "B");

  holeTileColIdx = 0;
  holeTileRowIdx = 0;
  moveTileColIdx = -1;
  moveTileRowIdx = -1;
  randomizeMoveTileInMillis = 300;
  initRandomizeTileStepCount = STARTOFF_RANDOMIZE_TILE_STEP_COUNT;

  dumbdisplay.log("... done creating board");
}

void randomizeTilesStep() {
  int canCount = checkCanMoveFromDirs(randomizeCanMoveFromDirs, randomizeCanMoveFromDir);
  randomizeCanMoveFromDir = randomizeCanMoveFromDirs[random(canCount)];
  int fromColIdx;
  int fromRowIdx;
  canMoveFromDirToFromIdxes(randomizeCanMoveFromDir, fromColIdx, fromRowIdx);
  int toColIdx = holeTileColIdx;
  int toRowIdx = holeTileRowIdx;
  int fromTileId = boardTileIds[fromRowIdx][fromColIdx];
  String fromTileLevelId = String(fromTileId);
  boardTileIds[fromRowIdx][fromColIdx] = boardTileIds[holeTileRowIdx][holeTileColIdx];
  boardTileIds[holeTileRowIdx][holeTileColIdx] = fromTileId;
  board->switchLevel(fromTileLevelId);
  int x = toColIdx * TILE_SIZE;
  int y = toRowIdx * TILE_SIZE;

  // move the anchor of the level to the destination in randomizeMoveTileInMillis
  board->setLevelAnchor(x, y, randomizeMoveTileInMillis);
  
  holeTileColIdx = fromColIdx;
  holeTileRowIdx = fromRowIdx;

  // since the tile will be moved to the destination in randomizeMoveTileInMillis, delay randomizeMoveTileInMillis here
  delay(randomizeMoveTileInMillis); 
  
  // make sure the tile is at the destination
  board->setLevelAnchor(x, y);
}


#ifdef SUGGEST_MAX_DEPTH
int calcBoardCost() {
  int cost = 0;
  for (int rowTileIdx = 0; rowTileIdx < TILE_COUNT; rowTileIdx++) {
    for (int colTileIdx = 0; colTileIdx < TILE_COUNT; colTileIdx++) {
      int tileId = colTileIdx + rowTileIdx * TILE_COUNT;
      int boardTileId = boardTileIds[rowTileIdx][colTileIdx];
      if (boardTileId != tileId) {
        int colIdx = boardTileId % TILE_COUNT;
        int rowIdx = boardTileId / TILE_COUNT;
        cost += abs(colIdx - colTileIdx) + abs(rowIdx - rowTileIdx);
      }
    }
  }
  return cost;
}
int tryMoveTile(int depth, short canMoveFromDir) {
  int fromColIdx;
  int fromRowIdx;
  int fromTileId;
  int prevHoldColIdx;
  int prevHoldRowIdx;
  int prevHoleTileId;
  canMoveFromDirToFromIdxes(canMoveFromDir, fromColIdx, fromRowIdx);
  fromTileId = boardTileIds[fromRowIdx][fromColIdx];
  prevHoldColIdx = holeTileColIdx;
  prevHoldRowIdx = holeTileRowIdx;
  prevHoleTileId = boardTileIds[holeTileRowIdx][holeTileColIdx];
  boardTileIds[holeTileRowIdx][holeTileColIdx] = fromTileId;
  boardTileIds[fromRowIdx][fromColIdx] = prevHoleTileId;
  holeTileColIdx = fromColIdx;
  holeTileRowIdx = fromRowIdx;
  int lowestBoardCost = calcBoardCost();
  if (lowestBoardCost > 0 && depth > 0) {
    short canMoveFromDirs[4];
    int canCount = checkCanMoveFromDirs(canMoveFromDirs, canMoveFromDir);
    if (canCount > 0) {
      for (int i = 0; i < canCount; i++) {
        short canMoveFromDir = canMoveFromDirs[i];
        int ndBoardCost = tryMoveTile(depth - 1, canMoveFromDir);
        if (ndBoardCost != -1 && (ndBoardCost < lowestBoardCost)) {
          lowestBoardCost = ndBoardCost;
        }
      }
    }
  }
  holeTileColIdx = prevHoldColIdx;
  holeTileRowIdx = prevHoldRowIdx;
  boardTileIds[holeTileRowIdx][holeTileColIdx] = prevHoleTileId;
  boardTileIds[fromRowIdx][fromColIdx] = fromTileId;
  return lowestBoardCost;
}
short suggestMoveDir() {
  int boardCost = calcBoardCost();
  if (boardCost == 0) {
    return -1;
  }
  short canMoveFromDirs[4];
  int canCount = checkCanMoveFromDirs(canMoveFromDirs);
  if (canCount == 0) {
    return -1;
  }
  int suggestedBoardCost = -1;
  short suggestedMoveDir = -1;
  for (int i = 0; i < canCount; i++) {
    int maxDepth = random(SUGGEST_MAX_DEPTH - SUGGEST_MIN_DEPTH + 1) + SUGGEST_MIN_DEPTH;
    short canMoveFromDir = canMoveFromDirs[i];
    int ndBoardCost = tryMoveTile(maxDepth, canMoveFromDir);
    dumbdisplay.logToSerial("$$$ ... tried canMoveFromDir: " + String(canMoveFromDir) + " @ cost: " + String(ndBoardCost) + " ...");
    if (ndBoardCost != -1) {
      bool takeIt = suggestedBoardCost == -1;
      if (!takeIt) {
        if (ndBoardCost < suggestedBoardCost) {
          takeIt = true;
        } else if (ndBoardCost == suggestedBoardCost) {
          if (random(2) == 0) {
            takeIt = true;
          }
        }
      }
      if (takeIt) {
        suggestedBoardCost = ndBoardCost;
        suggestedMoveDir = canMoveFromDir;
      }
    }
  }
  dumbdisplay.logToSerial("$$$ suggestedMoveDir: " + String(suggestedMoveDir) + " @ cost: " + String(suggestedBoardCost));
  return suggestedMoveDir;
}
bool moveAsSuggested(short suggestedMoveDir) {
  if (suggestedMoveDir != -1) {
      int fromColIdx;
      int fromRowIdx;
      canMoveFromDirToFromIdxes(suggestedMoveDir, fromColIdx, fromRowIdx);
      int prevHoleTileId = boardTileIds[holeTileRowIdx][holeTileColIdx];
      int prevHoleTileColIdx = holeTileColIdx;
      int prevHoleTileRowIdx = holeTileRowIdx;
      int fromTileId = boardTileIds[fromRowIdx][fromColIdx];
      String fromTileLevelId = String(fromTileId);
      boardTileIds[holeTileRowIdx][holeTileColIdx] = boardTileIds[fromRowIdx][fromColIdx];
      boardTileIds[fromRowIdx][fromColIdx] = prevHoleTileId;
      holeTileColIdx = fromColIdx;
      holeTileRowIdx = fromRowIdx;
      board->switchLevel(fromTileLevelId);
      board->setLevelAnchor(prevHoleTileColIdx * TILE_SIZE, prevHoleTileRowIdx * TILE_SIZE, suggestedMoveTileInMillis);
      delay(suggestedMoveTileInMillis);
      board->setLevelAnchor(prevHoleTileColIdx * TILE_SIZE, prevHoleTileRowIdx * TILE_SIZE);
      return true;
  } else {
    if (!suggestContinuously) {
      dumbdisplay.log("No suggested move!");
    }
    return false;
  }
}
bool suggestMove() {
  short suggestedMoveDir = suggestMoveDir();
  return moveAsSuggested(suggestedMoveDir);
}
#endif

#ifdef SUPPORT_AI
  #if defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT > 0
    short lastAISuggestedMoveDirs[AI_SUGGEST_LM_COUNT]; 
    int lastAISuggestedMoveCount;
  #endif  
short suggestMoveDirWithAI() {
  for (int rowTileIdx = 0; rowTileIdx < TILE_COUNT; rowTileIdx++) {
    for (int colTileIdx = 0; colTileIdx < TILE_COUNT; colTileIdx++) {
      int idx = colTileIdx + rowTileIdx * TILE_COUNT;
      int boardTileId = boardTileIds[rowTileIdx][colTileIdx];
      float f = (float) boardTileId / (float) (TILE_COUNT * TILE_COUNT);
      input->data.f[idx] = f;
    }
  }
  #if defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT > 0
    for (int i = 0; i < AI_SUGGEST_LM_COUNT; i++) {
      short prevMoveDir = -1;
      if (i < lastAISuggestedMoveCount) {
        prevMoveDir = lastAISuggestedMoveDirs[i];
      }
      if (prevMoveDir == -1) {
        prevMoveDir = -1; // -1 means don't care
      }
      input->data.f[TILE_COUNT * TILE_COUNT + i] = ((float) (1 + prevMoveDir)) / (float) (4 + 1); 
    }
  #endif
  // run the model on this input and make sure it succeeds
  long detectStartMillis = millis();
  TfLiteStatus invokeStatus = interpreter->Invoke();
  if (invokeStatus != kTfLiteOk) {
    error_reporter->Report("Invoke failed");
  }
  long detectTakenMillis = millis() - detectStartMillis;
  dumbdisplay.logToSerial("### AI took: " + String(detectTakenMillis) + " ms");
  TfLiteTensor* output = interpreter->output(0);
  short suggestedMoveDir = -1;
  short canMoves[4];
  int canCount = 0;
  float maxProb = 0;
  int total_move_category_count = 4;
  #ifdef AI_STAGED_STAGE_COUNT
    total_move_category_count *= AI_STAGED_STAGE_COUNT;
    int maxProbStage = 0;
  #endif  
  for (int i_cat = 0; i_cat < total_move_category_count; i_cat++) {
    float p = output->data.f[i_cat];
  #ifdef AI_STAGED_STAGE_COUNT
    int i = i_cat % 4;
  #else 
    int i = i_cat;
  #endif
    String desc;
    if (i == 0) {
      desc = "from left";
    } else if (i == 1) {
      desc = "from right";
    } else if (i == 2) {
      desc = "from up";
    } else {
      desc = "from down";
    }
    bool moveIsInvalid = false;
    if (i == 0 && holeTileColIdx == 0) {
      moveIsInvalid = true;
    } else if (i == 1 && holeTileColIdx == (TILE_COUNT - 1)) {
      moveIsInvalid = true;
    } else if (i == 2 && holeTileRowIdx == 0) {
      moveIsInvalid = true;
    } else if (i == 3 && holeTileRowIdx == (TILE_COUNT - 1)) {
      moveIsInvalid = true;
    }
    String logMsg = String(". ") + String(i) + " (" + String(desc) + "): " + String(p, 3);
    if (moveIsInvalid) {
      logMsg = logMsg + " (INVALID)"; 
    }
    dumbdisplay.logToSerial(logMsg);
    if (!moveIsInvalid) {
      if (p >= FB_ACCEPTABLE_AI_PROB) {
        canMoves[canCount++] = i;
      }
      if (suggestedMoveDir == -1 || p > maxProb) {
        suggestedMoveDir = i;
        maxProb = p;
  #ifdef AI_STAGED_STAGE_COUNT
        maxProbStage = i_cat / 4;
  #endif
      }
    }
  }
  if (coopMode != 0) {
    // AI+search
    if (maxProb < FB_SURE_AI_PROB) {
      if (canCount == 0 || random(2) == 0) {
        suggestedMoveDir = -1;  // fallback to search
      } else {
        suggestedMoveDir = canMoves[random(canCount)];
      }
    }
  }
  if (suggestedMoveDir != -1) {
  #ifdef AI_STAGED_STAGE_COUNT
    String logMsg = "AI suggestedMoveDir: " + String(suggestedMoveDir) + " with probability: " + String(maxProb, 3) + " {stage: " + String(maxProbStage) + "}";
  #else
    String logMsg = "AI suggestedMoveDir: " + String(suggestedMoveDir) + " with probability: " + String(maxProb, 3);
  #endif
    if (coopMode == 0) {
      logMsg = "'AI Only' " + logMsg;
    }
    dumbdisplay.logToSerial("### " + logMsg);
  } else {
    if (coopMode == 0) {
      dumbdisplay.log("XXX AI not suggesting valid move ==> fallback to search for move");
    } else {
      dumbdisplay.logToSerial("### !!! fallback to search for move");
    }
    suggestedMoveDir = suggestMoveDir();
  }
  #if defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT > 0
    if (lastAISuggestedMoveCount < AI_SUGGEST_LM_COUNT) {
      lastAISuggestedMoveDirs[lastAISuggestedMoveCount++] = suggestedMoveDir;
    } else {
      for (int i = 0; i < AI_SUGGEST_LM_COUNT - 1; i++) {
        lastAISuggestedMoveDirs[i] = lastAISuggestedMoveDirs[i + 1];
      }
      lastAISuggestedMoveDirs[AI_SUGGEST_LM_COUNT - 1] = suggestedMoveDir;
    }
    //lastAISuggestedMoveDir = suggestedMoveDir;
  #endif  
  return suggestedMoveDir;
}
bool suggestMoveWithAI() {
  short suggestedMoveDir = suggestMoveDirWithAI();
  return moveAsSuggested(suggestedMoveDir);
}

#endif

void ensureBoardInitialized() {
  if (holeTileColIdx == -1) {
    initializeBoard();
  }
#if defined(SUPPORT_AI) && defined(AI_SUGGEST_LM_COUNT) && AI_SUGGEST_LM_COUNT > 0
  lastAISuggestedMoveCount = 0;
#endif
}

void startRandomizeBoard(int stepCount) {
  showHideHoleTile(false);
  randomizeTilesStepCount = stepCount;//initRandomizeTileStepCount;
  randomizeCanMoveFromDir = -1;
}

int posToHoleTileFromDir(int x, int y) {
  if (y >= holeTileRowIdx * TILE_SIZE && y < (holeTileRowIdx + 1) * TILE_SIZE) {
    if (x < holeTileColIdx * TILE_SIZE) {
      if (x < (holeTileColIdx - 1) * TILE_SIZE) {
        return -1;
      } else {
        return 0 ;  // left
      }
    }
    if (x >= (holeTileColIdx + 1) * TILE_SIZE) {
      if (x >= (holeTileColIdx + 2) * TILE_SIZE) {
        return -1;
      } else {
        return 1;  // right
      }
    }
  }
  if (x >= holeTileColIdx * TILE_SIZE && x < (holeTileColIdx + 1) * TILE_SIZE) {
    if (y < holeTileRowIdx * TILE_SIZE) {
      if (y < (holeTileRowIdx - 1) * TILE_SIZE) {
        return -1;
      } else {
        return 2;  // up
      }
    }
    if (y >= (holeTileRowIdx + 1) * TILE_SIZE) {
      if (y >= (holeTileRowIdx + 2) * TILE_SIZE) {
        return -1;
      } else {
        return 3;  // down
      }
    }
  }
  return -1;
}

bool posToHoleTileFromIdxes(int x, int y, int& colIdx, int& rowIdx, int& fromDir) {
  colIdx = -1;
  rowIdx = -1;
  fromDir = posToHoleTileFromDir(x, y);
  if (fromDir == -1) {
    return false;
  }
  if (fromDir == 0) {
    colIdx = holeTileColIdx - 1;
    rowIdx = holeTileRowIdx;
  } else if (fromDir == 1) {
    colIdx = holeTileColIdx + 1;
    rowIdx = holeTileRowIdx;
  } else if (fromDir == 2) {
    colIdx = holeTileColIdx;
    rowIdx = holeTileRowIdx - 1;
  } else {
    colIdx = holeTileColIdx;
    rowIdx = holeTileRowIdx + 1;
  }
  return true;
}

bool onBoardDragged(int x, int y) {
  bool tileMoved = false;
  if (x != -1 && y != -1) {
    // dragging
    if (moveTileColIdx == -1) {
      int colIdx;
      int rowIdx;
      int fromDir;
      if (posToHoleTileFromIdxes(x, y, colIdx, rowIdx, fromDir)) {
        moveTileColIdx = colIdx;
        moveTileRowIdx = rowIdx;
        moveTileFromDir = fromDir;
        moveTileDelta = 0;
        moveTileRefX = x;
        moveTileRefY = y;
        moveTileId = boardTileIds[moveTileRowIdx][moveTileColIdx];
      }
    } else {
      int tileAnchorX = moveTileColIdx * TILE_SIZE;
      int tileAnchorY = moveTileRowIdx * TILE_SIZE;
      int delta;
      if (moveTileFromDir == 0) {
        delta = x - moveTileRefX;
        if (delta > 0) {
          if (delta > TILE_SIZE) {
            delta = TILE_SIZE;
          }
          tileAnchorX += delta;
        }
      } else if (moveTileFromDir == 1) {
        delta = moveTileRefX - x;
        if (delta > 0) {
          if (delta > TILE_SIZE) {
            delta = TILE_SIZE;
          }
          tileAnchorX -= delta;
        }
      } else if (moveTileFromDir == 2) {
        delta = y - moveTileRefY;
        if (delta > 0) {
          if (delta > TILE_SIZE) {
            delta = TILE_SIZE;
          }
          tileAnchorY += delta;
        }
      } else {
        delta = moveTileRefY - y;
        if (delta > 0) {
          if (delta > TILE_SIZE) {
            delta = TILE_SIZE;
          }
          tileAnchorY -= delta;
        }
      }
      board->switchLevel(String(moveTileId));
      board->setLevelAnchor(tileAnchorX, tileAnchorY);
      moveTileDelta = delta;
    }
  } else {
    // done dragging
    if (moveTileColIdx != -1) {
      int tileAnchorX;
      int tileAnchorY;
      if (moveTileDelta >= TILE_SIZE / 3) {
        tileAnchorX = holeTileColIdx * TILE_SIZE;
        tileAnchorY = holeTileRowIdx * TILE_SIZE;
        int prevHoleTileId = boardTileIds[holeTileRowIdx][holeTileColIdx];
        boardTileIds[holeTileRowIdx][holeTileColIdx] = boardTileIds[moveTileRowIdx][moveTileColIdx];
        boardTileIds[moveTileRowIdx][moveTileColIdx] = prevHoleTileId;
        holeTileColIdx = moveTileColIdx;
        holeTileRowIdx = moveTileRowIdx;
      } else {
        tileAnchorX = moveTileColIdx * TILE_SIZE;
        tileAnchorY = moveTileRowIdx * TILE_SIZE;
      }
      board->switchLevel(String(moveTileId));
      board->setLevelAnchor(tileAnchorX, tileAnchorY);
      tileMoved = true;
    }
    moveTileColIdx = -1;
    moveTileRowIdx = -1;
  }
  return tileMoved;
}

bool checkBoardSolved() {
  for (int rowTileIdx = 0; rowTileIdx < TILE_COUNT; rowTileIdx++) {
    for (int colTileIdx = 0; colTileIdx < TILE_COUNT; colTileIdx++) {
      int tileId = colTileIdx + rowTileIdx * TILE_COUNT;
      int boardTileId = boardTileIds[rowTileIdx][colTileIdx];
      if (boardTileId != tileId) {
        return false;
      }
    }
  }
  dumbdisplay.log("***** Board Solved *****");
  board->enableFeedback();
#ifdef SUGGEST_MAX_DEPTH
  suggestSelection->disabled(true);
  suggestSelection->deselect(1);
  suggestContinuously = false;
#endif
  showHideHoleTile(true);
  delay(200);
  showHideHoleTile(false);
  delay(200);
  showHideHoleTile(true);
  randomizeMoveTileInMillis -= 50;  // randomize faster and faster
  if (randomizeMoveTileInMillis < 50) {
    randomizeMoveTileInMillis = 50;
  }
  initRandomizeTileStepCount += 5;  // randomize more and more
  if (initRandomizeTileStepCount > 100) {
    initRandomizeTileStepCount = 100;
  }
  waitingToRestartMillis = 0;
  return true;
}


void initializeDD() {
  randomSeed(millis());

  board = dumbdisplay.createGraphicalLayer(BOARD_SIZE, BOARD_SIZE);
  board->backgroundColor("teal");
  board->border(8, "navy", "round", 5);
  board->drawRect(0, 0, BOARD_SIZE, BOARD_SIZE, "azure", true);
  board->drawRoundRect(20, 20, BOARD_SIZE - 40, BOARD_SIZE - 40, 10, "aqua", true);

  board->drawImageFileFit("dumbdisplay.png");
  board->setTextFont("DL::Roboto");
  board->drawTextLine("In God We Trust", 34, "C", "white", "purple", 32);     // C is for centering on the line (from left to right)
  board->drawTextLine("❤️ May God bless you ❤️", 340, "R-20", "purple", "", 20);  // R is for right-justify align on the line; with -20 offset from right

  board->enableFeedback();

#ifdef SUGGEST_MAX_DEPTH
  
  resetButton = dumbdisplay.createLcdLayer(20, 1);
  resetButton->border(1, "red", "round");
  resetButton->pixelColor("darkred");
  resetButton->backgroundColor("orange");
  #ifdef AI_STAGED_STAGE_COUNT
    resetButton->writeCenteredLine("🔄 Reset (" + String(AI_STAGED_STAGE_COUNT) + ") 🔄");
  #elif defined(AI_SUGGEST_LM_COUNT)
    resetButton->writeCenteredLine("🔄 Reset (" + String(AI_SUGGEST_LM_COUNT) + ") 🔄");
  #else
    resetButton->writeCenteredLine("🔄 Reset 🔄");
  #endif
  resetButton->enableFeedback();

  coopSelection = dumbdisplay.createSelectionLayer(11, 1, 3, 1);
  coopSelection->border(1, "darkblue", "round");
  coopSelection->pixelColor("black");
  coopSelection->backgroundColor("lightblue");
  coopSelection->textCentered("🤖 AI Only");
  coopSelection->textCentered("AI+Search", 0, 1);
  coopSelection->textCentered("Search Only", 0, 2);
  coopSelection->select(coopMode);

  suggestSelection = dumbdisplay.createSelectionLayer(11, 1, 2, 1);
  suggestSelection->border(2, "black");
  suggestSelection->pixelColor("darkgreen");
  suggestSelection->disabled(true);
  suggestSelection->text("💪 Suggest");
  suggestSelection->textRightAligned("Continuous", 0, 1);

  dumbdisplay.configAutoPin(
    DDAutoPinConfig('V').
      addLayer(resetButton).
      addLayer(board).
      addLayer(coopSelection).
      addLayer(suggestSelection).
    build());
  
  suggestContinuously = false;
  
#endif

  holeTileColIdx = -1;
  holeTileRowIdx = -1;
  randomizeTilesStepCount = 0;
  waitingToRestartMillis = 0;

  if (false) {
    ensureBoardInitialized();
    showHideHoleTile(false);
  }
}

void updateDD(bool isFirstUpdate) {

#ifdef SUGGEST_MAX_DEPTH  
  const DDFeedback* resetFeedback = resetButton->getFeedback();
  if (resetFeedback != NULL) {
    if (resetFeedback->type == DOUBLECLICK) {
      resetButton->flash();
      dumbdisplay.log("... resetting ...");
      delay(200);
      pdd.masterReset();
      return;
    } else {
      dumbdisplay.log("! double tab 'Reset' to reset !");
    }
  }
#endif  

  if (waitingToRestartMillis != -1) {
    // starts off waiting for double tab
    long nowMillis = millis();
    long diffMillis = nowMillis - waitingToRestartMillis;
    if (waitingToRestartMillis == 0 || diffMillis > 15000) {
      dumbdisplay.log("! double tab 'board' to start !");
      waitingToRestartMillis = nowMillis;
    }
  }

#ifdef SUGGEST_MAX_DEPTH
  const DDFeedback* coopFeedback = coopSelection->getFeedback();
  if (coopFeedback != NULL) {
    int x = coopFeedback->x;
    coopMode = x;
    coopSelection->select(coopMode);
  }
#endif


  const DDFeedback* boardFeedback = board->getFeedback();

#ifdef SUGGEST_MAX_DEPTH
  const DDFeedback* suggestFeedback = suggestSelection->getFeedback();
#endif

  if (randomizeTilesStepCount > 0) {
    // randomizing the board
    randomizeTilesStep();
    randomizeTilesStepCount--;
    if (randomizeTilesStepCount == 0) {
      // randomization is done
      dumbdisplay.log("... done randomizing board");
      board->enableFeedback(":drag");  // :drag to allow dragging that produces MOVE feedback type (and ended with -1, -1 MOVE feedbackv)
#ifdef SUGGEST_MAX_DEPTH
      suggestSelection->disabled(false);
#endif
    }
  } else {
    if (boardFeedback != NULL) {
      if (boardFeedback->type == DOUBLECLICK) {
        // double click ==> randomize the board, even during play
        board->flash();
        board->disableFeedback();
        ensureBoardInitialized();
        int randomizeStepCount = initRandomizeTileStepCount;
        dumbdisplay.log("Randomizing board by " + String(randomizeStepCount) + " steps ...");
        waitingToRestartMillis = -1;
        startRandomizeBoard(randomizeStepCount);
        return;
      } else if (boardFeedback->type == MOVE) {
        // dragging / moving a tile ... handle it in onBoardDragged
        if (onBoardDragged(boardFeedback->x, boardFeedback->y)) {
          // ended up moving a tile ... check if the board is solved
          checkBoardSolved();
        }
      }
    }
#ifdef SUGGEST_MAX_DEPTH 
    bool suggest = false;
    if (suggestFeedback != NULL) {
      int x = suggestFeedback->x;
      int y = suggestFeedback->y;
      if (x == 0 && y == 0) {
        suggest = true;
        suggestSelection->flashArea(0, 0);
      } else if (x == 1 && y == 0) {
        suggestContinuously = !suggestContinuously;
        suggestSelection->setSelected(suggestContinuously, 1);
        suggestSelection->flashArea(1, 0);
      }
    }
    if (suggestContinuously) {
      suggest = true;
    }
    if (suggest) {
      bool moved = coopMode == 2 ? suggestMove() : suggestMoveWithAI();
      if (moved) {
        checkBoardSolved();
      }
    }
#endif      
  }
}

void disconnectedDD() {
}

void handleIdle(bool justBecameIdle) {
}



void setup() {

#ifdef SUPPORT_AI
  Serial.begin(115200);

  dumbdisplay.logToSerial("Initialize TensorFlow Lite ...");
 
  // check version to make sure supported
  dumbdisplay.logToSerial("* check TensorFlow Lite version ...");
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report("Model provided is schema version %d not equal to supported version %d.",
    model->version(), TFLITE_SCHEMA_VERSION);
  }

  // allocation memory for tensor_arena
  dumbdisplay.logToSerial("* allocate heap memory for TensorFlow Lite ...");
  tensor_arena = (uint8_t *) heap_caps_malloc(tensor_arena_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (tensor_arena == NULL) {
    error_reporter->Report("heap_caps_malloc() failed");
    return;
  }

  // pull in all the operation implementations
  dumbdisplay.logToSerial("* create TensorFlow Lite resolver ...");
  tflite::AllOpsResolver* resolver = new tflite::AllOpsResolver();

  // build an interpreter to run the model with
  dumbdisplay.logToSerial("* create TensorFlow Lite interpreter ...");
  interpreter = new tflite::MicroInterpreter(model, *resolver, tensor_arena, tensor_arena_size, error_reporter);

  // allocate memory from the tensor_arena for the model's tensors
  dumbdisplay.logToSerial("* allocate memory for TensorFlow Lite interpreter ...");
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() failed");
    return;
  }

  // obtain a pointer to the model's input tensor
  dumbdisplay.logToSerial("* get input tensor from TensorFlow Lite interpreter ...");
  input = interpreter->input(0);

  dumbdisplay.logToSerial("... done initialize TensorFlow Lite");
 
#endif
}

void loop() {
  // standard way of using pdd  
  pdd.loop([](){
    // **********
    // *** initializeCallback ***
    // **********
    initializeDD();
  }, [](){
    // **********
    // *** updateCallback ***
    // **********
    updateDD(!pdd.firstUpdated());
  }, [](){
    // **********
    // *** disconnectedCallback ***
    // **********
    disconnectedDD();
  });
  if (pdd.isIdle()) {
    handleIdle(pdd.justBecameIdle());
  }
}