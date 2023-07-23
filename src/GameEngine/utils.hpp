#pragma once
// [bool] curently white's turn
#define whiteturn (board.sideToMove() == chess::Color::WHITE)

// [string] numeric name of move
#define movename(move) (chess::squareToString[move.from()] + chess::squareToString[move.to()])

// [bool] square is a light tile
#define lighttile(sqi) (((sq >> 3) ^ sq) & 1)