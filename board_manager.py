

import random
import textwrap

import numpy as np

class BoardManager:
  def __init__(self, tile_count: int):
    self.board_size = tile_count
    self.board_tiles = [i for i in range(tile_count * tile_count)]
    self.hole_tile_col_idx = 0
    self.hole_tile_row_idx = 0
    self.randomize_can_move_from_dirs = [0, 0, 0, 0]
    self.randomize_can_move_from_dir = -1
  def clone(self):
    new_board_manager = BoardManager(self.board_size)
    new_board_manager.board_tiles = self.board_tiles.copy()
    new_board_manager.hole_tile_col_idx = self.hole_tile_col_idx
    new_board_manager.hole_tile_row_idx = self.hole_tile_row_idx
    new_board_manager.randomize_can_move_from_dirs = self.randomize_can_move_from_dirs.copy()
    new_board_manager.randomize_can_move_from_dir = self.randomize_can_move_from_dir
    return new_board_manager   
  def show_board(self):
    for colIdx in range(self.board_size):
      for rowIdx in range(self.board_size):
        tileIdx = colIdx * self.board_size + rowIdx
        tile_value = self.board_tiles[tileIdx]
        if rowIdx == 0:
          if colIdx == 0:
            print(self.board_size * "-----" + "-")
          print("|", end="")
        print(f" {tile_value:2} |", end="")
        if rowIdx == (self.board_size - 1):
          print()
          if colIdx == (self.board_size - 1):
            print(self.board_size * "-----" + "-")
      #print()
  def randomize_board_step(self) -> int:
    can_count = self._check_can_move_from_dirs(self.randomize_can_move_from_dir)
    self.randomize_can_move_from_dir = self.randomize_can_move_from_dirs[random.randint(0, can_count - 1)]
    (from_col_idx, from_row_idx) = self._can_move_from_dir_to_from_idxes(self.randomize_can_move_from_dir)
    to_col_idx = self.hole_tile_col_idx
    to_row_idx = self.hole_tile_row_idx
    from_tile_value = self.board_tiles[from_row_idx * self.board_size + from_col_idx]
    self.board_tiles[from_row_idx * self.board_size + from_col_idx] = self.board_tiles[self.hole_tile_row_idx * self.board_size + self.hole_tile_col_idx]
    self.board_tiles[self.hole_tile_row_idx * self.board_size + self.hole_tile_col_idx] = from_tile_value
    self.hole_tile_col_idx = from_col_idx
    self.hole_tile_row_idx = from_row_idx
    move = self.randomize_can_move_from_dir
    if move == 0:
      move = 1
    elif move == 2:
      move = 3
    elif move == 3:
      move = 2
    else:
      move = 0
    return move
  def _check_can_move_from_dirs(self, prev_can_move_from_dir) -> int:
    canCount = 0
    if self.hole_tile_col_idx > 0 and prev_can_move_from_dir != 1:
      self.randomize_can_move_from_dirs[canCount] = 0;  # 0: left
      canCount += 1
    if self.hole_tile_col_idx < (self.board_size - 1) and prev_can_move_from_dir != 0:
      self.randomize_can_move_from_dirs[canCount] = 1;  # 1: right
      canCount += 1
    if self.hole_tile_row_idx > 0 and prev_can_move_from_dir != 3:
      self.randomize_can_move_from_dirs[canCount] = 2;  # 2: up
      canCount += 1
    if self.hole_tile_row_idx < (self.board_size - 1) and prev_can_move_from_dir != 2:
      self.randomize_can_move_from_dirs[canCount] = 3;  # 3: down
      canCount += 1
    return canCount 
  def _can_move_from_dir_to_from_idxes(self, can_move_from_dir):
    if can_move_from_dir == 0:
      from_col_idx = self.hole_tile_col_idx - 1
      from_row_idx = self.hole_tile_row_idx
    elif can_move_from_dir == 1:
      from_col_idx = self.hole_tile_col_idx + 1
      from_row_idx = self.hole_tile_row_idx
    elif can_move_from_dir == 2:
      from_col_idx = self.hole_tile_col_idx
      from_row_idx = self.hole_tile_row_idx - 1
    else:
      from_col_idx = self.hole_tile_col_idx
      from_row_idx = self.hole_tile_row_idx + 1
    return (from_col_idx, from_row_idx)


def bytes_to_c_arr(data, lowercase=True):
    return [format(b, '#04x' if lowercase else '#04X') for b in data]

def test_it():
  if __name__ == "__main__":
    board_size = 5
    move_category_count = 4
    sample_count = 3
    show_values = sample_count < 5

    randomize_board_helper = BoardManager(board_size)

    x_values = np.empty((0, board_size * board_size),int)
    y_values = np.empty(0, int)

    if show_values:
      randomize_board_helper.show_board()
    for i in range(sample_count):
      move = randomize_board_helper.randomize_board_step()
      if show_values:
        print(f"move: {move}")
        randomize_board_helper.show_board()
      x_values = np.append(x_values, [randomize_board_helper.board_tiles], axis=0)
      y_values = np.append(y_values, move)

    #y_values = keras.utils.to_categorical(y_values, move_categories)

    print(f"* x_values shape: {x_values.shape}")
    if show_values:
      print(f"* x_values:")
      print(textwrap.indent(str(x_values), "    "))

    print(f"* y_values shape: {y_values.shape}")
    if show_values:
      print(f"* y_values:")
      print(textwrap.indent(str(y_values), "    "))


def write_model_as_c_header_file(model_bytes, header_file_path, model_var_name):
  byte_count = len(model_bytes)
  line_byte_count = 12
  with open(header_file_path, "w") as f:
    f.write("const unsigned char {}[] = {{\n".format(model_var_name))
    for i in range(0, byte_count, line_byte_count):
      if i > 0:
        f.write(",\n")
      bytes = model_bytes[i:i+line_byte_count]    
      f.write("\t{}".format(", ".join(bytes_to_c_arr(bytes))))
    f.write("\n};\n")


   