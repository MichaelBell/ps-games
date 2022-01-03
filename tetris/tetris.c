#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#include "display.h"

#include "hardware/gpio.h"

PS_DISPLAY disp;

#define BOX_SIZE 10

// Sprite Data
const uint32_t sd_one_box_red[BOX_SIZE * BOX_SIZE / 2] =
{
  0x80008000, 0x80008000, 0x80008000, 0x80008000, 0x80008000,
  0x8000a000, 0xa000a000, 0xa000a000, 0xa000a000, 0xa0008000,
  0x8000a000, 0xd000d000, 0xd000d000, 0xd000d000, 0xa0008000,
  0x8000a000, 0xd000e0c8, 0xe0c8e0c8, 0xe0c8d000, 0xa0008000,
  0x8000a000, 0xd000e0c8, 0xe0c8e0c8, 0xe0c8d000, 0xa0008000,
  0x8000a000, 0xd000e0c8, 0xe0c8e0c8, 0xe0c8d000, 0xa0008000,
  0x8000a000, 0xd000e0c8, 0xe0c8e0c8, 0xe0c8d000, 0xa0008000,
  0x8000a000, 0xd000d000, 0xd000d000, 0xd000d000, 0xa0008000,
  0x8000a000, 0xa000a000, 0xa000a000, 0xa000a000, 0xa0008000,
  0x80008000, 0x80008000, 0x80008000, 0x80008000, 0x80008000
};
const uint32_t sd_one_box_green[BOX_SIZE * BOX_SIZE / 2] =
{
  0x04000400, 0x04000400, 0x04000400, 0x04000400, 0x04000400,
  0x04000500, 0x05000500, 0x05000500, 0x05000500, 0x05000400,
  0x04000500, 0x06000600, 0x06000600, 0x06000600, 0x05000400,
  0x04000500, 0x06003706, 0x37063706, 0x37060600, 0x05000400,
  0x04000500, 0x06003706, 0x37063706, 0x37060600, 0x05000400,
  0x04000500, 0x06003706, 0x37063706, 0x37060600, 0x05000400,
  0x04000500, 0x06003706, 0x37063706, 0x37060600, 0x05000400,
  0x04000500, 0x06000600, 0x06000600, 0x06000600, 0x05000400,
  0x04000500, 0x05000500, 0x05000500, 0x05000500, 0x05000400,
  0x04000400, 0x04000400, 0x04000400, 0x04000400, 0x04000400
};
const uint32_t* sd_box_data[] = { sd_one_box_red, sd_one_box_green };

#define SHAPE_LINE   0
#define SHAPE_LEFTL  1
#define SHAPE_RIGHTL 2
#define SHAPE_LEFTZ  3
#define SHAPE_RIGHTZ 4
#define SHAPE_TEE    5
#define SHAPE_SQUARE 6
#define NUM_SHAPES   7

const uint8_t layout[NUM_SHAPES] =
{
  0b11110000, 0b11100010, 0b11101000,
  0b11000110, 0b01101100, 0b11100100, 0b11001100
};

// Pieces, made of 4 individual sprites 
typedef struct
{
  uint8_t shape;
  psd_sprite* sprites[4];
} Piece;

#define ROWS 24
#define COLS 10

#define MAX_BOXES (ROWS * COLS)
psd_sprite* landed_boxes[MAX_BOXES];

void make_piece(Piece* piece, uint8_t shape)
{
  piece->shape = shape;

  int j = 0;
  psd_vec offset = { 3 * BOX_SIZE, 0 };
  const psd_vec size = { BOX_SIZE, BOX_SIZE };
  const uint32_t* sd_box_data_ptr = sd_box_data[rand() % 2];
  for (int i = 7; i > 0; --i)
  {
    if ((1 << i) & layout[shape])
    {
      piece->sprites[j++] = ps_display_add_sprite(&disp, offset, size, sd_box_data_ptr, sizeof(sd_one_box_red) / sizeof(uint32_t));
    }
    if (i == 4)
    {
      offset.x = 3 * BOX_SIZE;
      offset.y = BOX_SIZE;
    }
    else
    {
      offset.x += BOX_SIZE;
    }
  }
}

void move_piece(Piece* piece, psd_vec offset)
{
  for (int i = 0; i < 4; ++i)
  {
    psd_vec newpos = piece->sprites[i]->pos;
    newpos.x += offset.x;
    newpos.y += offset.y;
    ps_display_move_sprite(&disp, piece->sprites[i], newpos);
  }
}

bool check_position(psd_vec pos)
{
  if (pos.x < 0 || pos.x > (COLS - 1) * BOX_SIZE ||
      pos.y < 0 || (pos.y + BOX_SIZE) > ROWS * BOX_SIZE)
  {
    return false;
  }

  int boxloc = (pos.x / BOX_SIZE) + (pos.y / BOX_SIZE) * COLS;
  if (landed_boxes[boxloc] != NULL)
  {
    return false;
  }
  boxloc = (pos.x / BOX_SIZE) + ((pos.y + BOX_SIZE - 1) / BOX_SIZE) * COLS;
  if (landed_boxes[boxloc] != NULL)
  {
    return false;
  }

  return true;
}

bool move_allowed(Piece* piece, psd_vec offset)
{
  for (int i = 0; i < 4; ++i)
  {
    psd_vec newpos = piece->sprites[i]->pos;
    newpos.x += offset.x;
    newpos.y += offset.y;

    if (!check_position(newpos)) return false;
  }

  return true;
}

void rotate_piece(Piece* piece, bool left)
{
  int centre_idx = 1;
  switch (piece->shape)
  {
    case SHAPE_SQUARE: 
      return;
    
    case SHAPE_RIGHTZ:
      centre_idx = 0;
    case SHAPE_LEFTZ:
    case SHAPE_LINE:
      // For these shapes just flip between two orientations to avoid wobble.
      left = (piece->sprites[0]->pos.y == piece->sprites[1]->pos.y);
      break;
  }

  psd_vec rotate_centre = piece->sprites[centre_idx]->pos;
  for (int j = 0; j < 2; ++j)
  {
    for (int i = 0; i < 4; ++i)
    {
      if (i == centre_idx) continue;

      psd_sprite* sprite = piece->sprites[i];
      psd_vec offset;
      offset.y = -(sprite->pos.x - rotate_centre.x);
      offset.x = sprite->pos.y - rotate_centre.y;
      if (!left)
      {
        offset.x = -offset.x;
        offset.y = -offset.y;
      }
      offset.x += rotate_centre.x;
      offset.y += rotate_centre.y;

      if (j == 1) 
      {
        // Already checked all sprites, actually do the move
        ps_display_move_sprite(&disp, sprite, offset);
      }
      else if (!check_position(offset))
      {
        // This box would go out of bounds. Fail
        return;
      }
    }
  }
}

void land_piece(Piece* piece, psd_vec offset)
{
  int landed_rows[4];

  for (int i = 0; i < 4; ++i)
  {
    psd_vec pos = piece->sprites[i]->pos;
    if (offset.x != 0)
    {
      pos.x += offset.x;
      ps_display_move_sprite(&disp, piece->sprites[i], pos);
    }

    int row = pos.y / BOX_SIZE;
    landed_rows[i] = row;    
    int boxloc = (pos.x / BOX_SIZE) + row * COLS;
    landed_boxes[boxloc] = piece->sprites[i];
  }

  int removed_rows[4];
  int removed_row_count = 0;
  for (int i = 0; i < 4; ++i)
  {
    if (i > 0 && landed_rows[i] == landed_rows[i-1]) continue;

    bool remove_row = true;
    int boxloc = landed_rows[i] * COLS;
    for (int j = 0; j < COLS; ++j, ++boxloc)
    {
      if (!landed_boxes[boxloc])
      {
        remove_row = false;
        break;
      }
    }

    if (remove_row)
    {
      boxloc = landed_rows[i] * COLS;
      for (int j = 0; j < COLS; ++j, ++boxloc)
      {
        ps_display_remove_sprite(&disp, landed_boxes[boxloc]);
        landed_boxes[boxloc] = NULL;
      }

      // Removed rows is sorted from highest to lowest.
      int j = removed_row_count;
      for (; j > 0 && removed_rows[j-1] < landed_rows[i]; --j)
      {
        removed_rows[j] = removed_rows[j-1];
      }
      removed_rows[j] = landed_rows[i];
      ++removed_row_count;
    }
  }

  if (removed_row_count)
  {
    int removed_row_idx = 1;
    for (int i = removed_rows[0] - 1; i >= 0; --i)
    {
      if (removed_row_idx < removed_row_count && i == removed_rows[removed_row_idx]) removed_row_idx++;

      int newboxloc = (i + removed_row_idx) * COLS;
      int oldboxloc = i * COLS;
      for (int j = 0; j < COLS; ++j, ++newboxloc, ++oldboxloc)
      {
        if (landed_boxes[oldboxloc])
        {
          psd_vec pos = landed_boxes[oldboxloc]->pos;
          pos.y += BOX_SIZE * removed_row_idx;
          ps_display_move_sprite(&disp, landed_boxes[oldboxloc], pos);
          landed_boxes[newboxloc] = landed_boxes[oldboxloc];
          landed_boxes[oldboxloc] = NULL;
        }
      }
    }
  }
}

int main()
{
  stdio_init_all();

  gpio_init_mask(0xffe000);
  gpio_set_dir_in_masked(0xff0000);
  gpio_set_dir_out_masked(0x00e000);
  for (uint32_t i = 16; i < 24; i++)
  {
    gpio_pull_up(i);
  }

  ps_display_init(&disp, PSD_BLACK);

  memset(landed_boxes, 0, sizeof(landed_boxes));

  {
    psd_vec boundtop = {COLS * BOX_SIZE, 0};
    psd_vec boundsize = {1, 240};
    ps_display_draw_frect(&disp, boundtop, boundsize, PSD_WHITE);
  }

  Piece current_piece;
  make_piece(&current_piece, rand() % NUM_SHAPES);

  ps_display_render(&disp);
  ps_display_finish_render(&disp);

  const uint32_t PRESS_DELAY = 8;
  const uint32_t MOVE_DELAY = 4;
  uint32_t rotate_count = PRESS_DELAY;
  uint32_t move_count = MOVE_DELAY;
  uint8_t last_side_move = 0;
  while (1)
  {
    if (--rotate_count == 0)
    {
      bool rotate = false;
      bool left = false;
      rotate_count = 1;

      if (gpio_get(PICOSYSTEM_SW_A_PIN) == 0) 
      {
        rotate = true;
      }
      else if (gpio_get(PICOSYSTEM_SW_Y_PIN) == 0)
      {
        rotate = true;
        left = true;
      }

      if (rotate)
      {
        rotate_count = PRESS_DELAY;

        for (int i = 0; i < NUM_SHAPES; ++i)
        {
          rotate_piece(&current_piece, left);
        }
      }
    }

    psd_vec offset = { 0, 1 };
    if (--move_count == 0)
    {
      move_count = 1;

      if (gpio_get(PICOSYSTEM_SW_LEFT_PIN) == 0)
      {
        offset.x -= 10; 
        if (!move_allowed(&current_piece, offset)) offset.x = 0;
      }
      else if (gpio_get(PICOSYSTEM_SW_RIGHT_PIN) == 0)
      {
        offset.x += 10; 
        if (!move_allowed(&current_piece, offset)) offset.x = 0;
      }

      if (offset.x != 0)
      {
        // Allow constant sliding sideways faster after an initial delay
        // to avoid accidentally moving twice when just doing a quick press
        if (last_side_move == 0) move_count = PRESS_DELAY;
        else move_count = MOVE_DELAY;
      }
      last_side_move = offset.x;
    }

    if (gpio_get(PICOSYSTEM_SW_DOWN_PIN) == 0)
    {
      offset.y = 4; 
      if (!move_allowed(&current_piece, offset)) offset.y = 1;
    }

    if (!move_allowed(&current_piece, offset))
    {
      land_piece(&current_piece, offset);
      make_piece(&current_piece, rand() % NUM_SHAPES);
    }
    else
    {
      move_piece(&current_piece, offset);
    }

    ps_display_render(&disp);
    ps_display_finish_render(&disp);
  }

  return 0;
}