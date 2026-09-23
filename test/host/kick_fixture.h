#pragma once

// Host-only kick fixture loader. Parses the text format defined by the
// JLSTZ kick task: comments, case/piece/from/turn/origin/window lines,
// exactly (lastRow - firstRow + 1) `row` lines, then one `expect` line.

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "framework.h"
#include "stackfall/core/kicks.h"

namespace sf {
namespace test {

struct KickCase {
  std::string name;
  PieceId piece;
  int fromState;  // 0..3
  Turn turn;
  int originCol;
  int originRow;
  Board board;
  bool expectOk;
  int expectKick;
  int expectCol;
  int expectRow;
};

inline void kickFail(const char* file, int line, const std::string& text) {
  Failure f;
  std::snprintf(f.msg, sizeof(f.msg), "%s:%d: %s", file, line, text.c_str());
  throw f;
}

inline PieceId parseKickPiece(const std::string& s, const char* file, int line) {
  if (s == "I") return PieceId::I;
  if (s == "J") return PieceId::J;
  if (s == "L") return PieceId::L;
  if (s == "O") return PieceId::O;
  if (s == "S") return PieceId::S;
  if (s == "T") return PieceId::T;
  if (s == "Z") return PieceId::Z;
  kickFail(file, line, "bad piece '" + s + "'");
  return PieceId::T;
}

inline int parseKickFrom(const std::string& s, const char* file, int line) {
  if (s == "0") return 0;
  if (s == "R") return 1;
  if (s == "2") return 2;
  if (s == "L") return 3;
  kickFail(file, line, "bad from '" + s + "'");
  return 0;
}

inline Turn parseKickTurn(const std::string& s, const char* file, int line) {
  if (s == "CW") return Turn::CW;
  if (s == "CCW") return Turn::CCW;
  kickFail(file, line, "bad turn '" + s + "'");
  return Turn::CW;
}

inline std::vector<KickCase> loadKickFixtures(const char* path, const char* file,
                                             int line) {
  std::ifstream in(path);
  if (!in) {
    kickFail(file, line, std::string("cannot open fixture '") + path + "'");
  }
  std::vector<KickCase> out;
  KickCase cur;
  bool inCase = false;
  bool havePiece = false, haveFrom = false, haveTurn = false;
  bool haveOrigin = false, haveWindow = false;
  int firstRow = 0, lastRow = -1, rowsSeen = 0;
  int lineno = 0;

  auto finishCase = [&]() {
    if (!inCase) return;
    if (!havePiece || !haveFrom || !haveTurn || !haveOrigin || !haveWindow) {
      kickFail(file, line, "incomplete case '" + cur.name + "'");
    }
    if (rowsSeen != lastRow - firstRow + 1) {
      kickFail(file, line, "row count mismatch in case '" + cur.name + "'");
    }
    out.push_back(cur);
    inCase = false;
  };

  std::string raw;
  while (std::getline(in, raw)) {
    ++lineno;
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    if (raw.empty() || raw[0] == '#') continue;
    if (raw.compare(0, 5, "case ") == 0) {
      finishCase();
      cur = KickCase();
      cur.name = raw.substr(5);
      cur.expectOk = false;
      cur.expectKick = -1;
      inCase = true;
      havePiece = haveFrom = haveTurn = haveOrigin = haveWindow = false;
      rowsSeen = 0;
    } else if (raw.compare(0, 6, "piece ") == 0) {
      cur.piece = parseKickPiece(raw.substr(6), file, line);
      havePiece = true;
    } else if (raw.compare(0, 5, "from ") == 0) {
      cur.fromState = parseKickFrom(raw.substr(5), file, line);
      haveFrom = true;
    } else if (raw.compare(0, 5, "turn ") == 0) {
      cur.turn = parseKickTurn(raw.substr(5), file, line);
      haveTurn = true;
    } else if (raw.compare(0, 7, "origin ") == 0) {
      int c = 0, r = 0;
      if (std::sscanf(raw.c_str() + 7, "%d %d", &c, &r) != 2) {
        kickFail(file, line, "bad origin line " + std::to_string(lineno));
      }
      cur.originCol = c;
      cur.originRow = r;
      haveOrigin = true;
    } else if (raw.compare(0, 7, "window ") == 0) {
      if (std::sscanf(raw.c_str() + 7, "%d %d", &firstRow, &lastRow) != 2 ||
          lastRow < firstRow) {
        kickFail(file, line, "bad window line " + std::to_string(lineno));
      }
      haveWindow = true;
    } else if (raw.compare(0, 4, "row ") == 0) {
      std::string cells = raw.substr(4);
      if (static_cast<int>(cells.size()) != Board::kWidth) {
        kickFail(file, line, "bad row width in case '" + cur.name + "'");
      }
      int r = firstRow + rowsSeen;
      for (int c = 0; c < Board::kWidth; ++c) {
        if (cells[static_cast<size_t>(c)] == '#') {
          cur.board.set(c, r, Cell::Z);  // garbage
        } else if (cells[static_cast<size_t>(c)] == '.') {
          cur.board.set(c, r, Cell::Empty);
        } else {
          kickFail(file, line, "bad cell in case '" + cur.name + "'");
        }
      }
      ++rowsSeen;
    } else if (raw.compare(0, 7, "expect ") == 0) {
      std::string e = raw.substr(7);
      if (e == "fail") {
        cur.expectOk = false;
        cur.expectKick = -1;
      } else {
        int k = -1, c = 0, r = 0;
        if (std::sscanf(e.c_str(), "ok %d %d %d", &k, &c, &r) != 3) {
          kickFail(file, line, "bad expect line " + std::to_string(lineno));
        }
        cur.expectOk = true;
        cur.expectKick = k;
        cur.expectCol = c;
        cur.expectRow = r;
      }
    } else {
      kickFail(file, line, "unknown directive '" + raw + "'");
    }
  }
  finishCase();
  return out;
}

// Runs one case through tryRotate; on mismatch throws Failure naming the
// case, the transition, expected vs actual (kickIndex, col, row) and the
// shapeString of both states.
inline void runKickCase(const KickCase& c, const char* file, int line) {
  ActivePiece piece{c.piece, static_cast<uint8_t>(c.fromState),
                    static_cast<int8_t>(c.originCol),
                    static_cast<int8_t>(c.originRow)};
  Rot from = static_cast<Rot>(c.fromState);
  Rot to = rotate(from, c.turn);
  int idx = transitionIndex(from, to);
  int kick = -99;
  bool ok = tryRotate(c.board, piece, c.turn, kick);
  if (ok != c.expectOk || (ok && (kick != c.expectKick ||
                                  piece.col != c.expectCol ||
                                  piece.row != c.expectRow))) {
    std::array<char, 17> a = shapeString(c.piece, c.fromState);
    std::array<char, 17> b = shapeString(c.piece, static_cast<int>(to));
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "kick case '%s' %s: expected %s %d (%d,%d), got %s %d "
                  "(%d,%d); from %s to %s",
                  c.name.c_str(), transitionName(idx),
                  c.expectOk ? "ok" : "fail", c.expectKick, c.expectCol,
                  c.expectRow, ok ? "ok" : "fail", kick, piece.col,
                  piece.row, a.data(), b.data());
    kickFail(file, line, buf);
  }
}

}  // namespace test
}  // namespace sf
