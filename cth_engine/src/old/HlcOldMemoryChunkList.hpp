#pragma once

#define TETRIS_GAME_OBJ_CHUNKS \
STATIC_TETRIS_GRID_VERTICES, DYNAMIC_TETRIS_GRID_INDICES, \
STATIC_TETRIS_GRID_LINE_VERTICES, STATIC_TETRIS_GRID_LINE_INDICES

#define PIECE_OBJ_CHUNKS \
DYNAMIC_PIECE_INDICES


namespace cth {
enum class Memory_Chunk_List : unsigned int {
	TETRIS_GAME_OBJ_CHUNKS,

	PIECE_OBJ_CHUNKS,

	SIZE,
	NONE
};

}
//TEMP delete or cleanup