#include <stdio.h>
#include <stdlib.h>
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

void make_piece(Piece* piece, uint8_t shape)
{
  piece->shape = shape;

  int j = 0;
  psd_vec offset = { 0, 0 };
  const psd_vec size = { BOX_SIZE, BOX_SIZE };
  for (int i = 7; i > 0; --i)
  {
    if ((1 << i) & layout[shape])
    {
      piece->sprites[j++] = ps_display_add_sprite(&disp, offset, size, sd_one_box_red, sizeof(sd_one_box_red) / sizeof(uint32_t));
    }
    if (i == 4)
    {
      offset.x = 0;
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
    ps_display_move_sprite(&disp, sprite, offset);
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

  Piece test_piece[NUM_SHAPES];
  for (int i = 0; i < NUM_SHAPES; ++i)
  {
    make_piece(&test_piece[i], i);

    psd_vec offset = { (3 * i + 2) * BOX_SIZE, (3 * i + 2) * BOX_SIZE };
    move_piece(&test_piece[i], offset);
  }

  ps_display_render(&disp);
  ps_display_finish_render(&disp);

  while (1)
  {
    bool rotate = false;
    bool left = false;
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
      for (int i = 0; i < NUM_SHAPES; ++i)
      {
        rotate_piece(&test_piece[i], left);
      }
    }

    ps_display_render(&disp);
    ps_display_finish_render(&disp);

    if (rotate)
    {
      sleep_ms(200);
    }
  }

  return 0;
}