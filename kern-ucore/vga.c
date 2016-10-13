/*
 * $File: vga.c
 * $Date: Fri Dec 20 19:08:32 2013 +0800
 * $Author: jiakai <jia.kai66@gmail.com>
 */

#include "font.h"

#define VGA_MEM_NRCOL 512
#define COLOR 255
#define NR_ROW (300 / CHAR_HEIGHT)
#define NR_COL (400 / CHAR_WIDTH)

typedef volatile unsigned* mem_ptr_t;

#ifdef SIMULATION
#include <stdlib.h>
static mem_ptr_t vga;
#else
static mem_ptr_t const vga = (mem_ptr_t)0xBA000000;
#endif

static char buf_mem[NR_ROW][NR_COL], actual_screen[NR_ROW][NR_COL],
			*buf[NR_ROW];
static int row, col;

static void render_char(int ch, int row, int col) {
	if (ch < ' ' || ch > 127)
		ch = ' ';
	buf[row][col] = ch;
	if (actual_screen[row][col] == ch)
		return;
	const unsigned *bitmap = CHAR_FONT_BITMAP[ch - ' '];
	unsigned cur_bitmap = bitmap[0];
	int i, j, pos = 0;
	mem_ptr_t dest = vga +
		row * VGA_MEM_NRCOL * CHAR_HEIGHT + col * CHAR_WIDTH;
	for (i = 0; i < CHAR_HEIGHT; i ++) {
		for (j = 0; j < CHAR_WIDTH; j ++, pos ++) {
			if (pos == 32) {
				pos = 0;
				cur_bitmap = *(++ bitmap);
			}
			if (cur_bitmap & 1)
				*dest = COLOR;
			else
				*dest = 0;
			dest ++;
			cur_bitmap >>= 1;
		}
		dest += VGA_MEM_NRCOL - CHAR_WIDTH;
	}
	actual_screen[row][col] = ch;
}

void vga_init() {
	int i, j;
	for (i = 0; i < NR_ROW; i ++) {
		buf[i] = buf_mem[i];
		for (j = 0; j < NR_COL; j ++) {
			buf_mem[i][j] = 0;
			actual_screen[i][j] = -1;
			render_char(0, i, j);
		}
	}
	row = 0;
	col = 0;
	render_char('_', 0, 0);
}

void vga_redraw() {
	int i, j;
	for (i = 0; i < NR_ROW; i ++)
		for (j = 0; j < NR_COL; j ++) {
			actual_screen[i][j] = -1;
			render_char(buf[i][j], i, j);
		}
}

void vga_putch(int ch) {
	static int prev_ch;
	if (prev_ch == '\r' && ch == '\n') {
		prev_ch = ch;
		return;
	}
	prev_ch = ch;
	if (ch == '\b') {
		render_char(' ', row, col);
		if (col)
			col --;
		render_char('_', row, col);
		return;
	}
	render_char(ch, row, col);
	col ++;
	if (ch == '\r' || ch == '\n' || col == NR_COL) {
		row ++;
		col = 0;
		if (row == NR_ROW) {
			row --;
			char *tmp = buf[0];
			int i, j;
			for (i = 1; i < NR_ROW; i ++)
				buf[i - 1] = buf[i];
			buf[row] = tmp;
			for (i = 0; i < NR_COL; i ++)
				tmp[i] = 0;

			for (i = 0; i < NR_ROW; i ++)
				for (j = 0; j < NR_COL; j ++)
					render_char(buf[i][j], i, j);
		}
	}
	render_char('_', row, col);
}

#ifdef USER_PROG

#include <file.h>
#include <stdio.h>

int main() {
	vga_init();
	for (; ;) {
		char ch;
		int ret = read(0, &ch, sizeof(char));
		if (ret != 1 || ch == 'q')
			return 0;
		cprintf("ch=%c (%x)\n", ch, ch);
		vga_putch(ch);
	}
}
#endif

#ifdef SIMULATION
int main() {
	vga =  (mem_ptr_t)malloc(4 * 512 * 300);
	vga_init();
	int i;
	for (i = 0; i < 10000; i ++)
		vga_putch(i);
	return 0;
}
#endif


