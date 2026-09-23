#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "stackfall/core/piece.h"
#include "stackfall/ui/layout.h"
#include "stackfall/ui/screens.h"

namespace sf::test {
namespace ascii_detail {

class Writer {
 public:
  Writer(char* out, std::size_t capacity)
      : out_(out), capacity_(capacity) {
    terminate();
  }

  void character(char value) {
    if (out_ != nullptr && length_ + 1u < capacity_) {
      out_[length_] = value;
    }
    ++length_;
    terminate();
  }

  void text(const char* value) {
    if (value == nullptr) return;
    for (std::size_t i = 0; value[i] != '\0'; ++i) {
      character(value[i]);
    }
  }

  void unsignedValue(uint32_t value) {
    char digits[10]{};
    std::size_t count = 0;
    do {
      digits[count++] = static_cast<char>('0' + value % 10u);
      value /= 10u;
    } while (value != 0u);
    while (count > 0) character(digits[--count]);
  }

  void newline() { character('\n'); }

  int finish() const {
    constexpr std::size_t kIntMax =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return length_ > kIntMax ? std::numeric_limits<int>::max()
                             : static_cast<int>(length_);
  }

 private:
  void terminate() {
    if (out_ == nullptr || capacity_ == 0) return;
    const std::size_t at =
        length_ < capacity_ ? length_ : capacity_ - 1u;
    out_[at] = '\0';
  }

  char* out_;
  std::size_t capacity_;
  std::size_t length_ = 0;
};

constexpr char paletteLetter(uint8_t palette, char empty) {
  constexpr char kLetters[] = ".IJLOSTZ";
  if (palette == 0u) return empty;
  return palette < sizeof(kLetters) - 1u ? kLetters[palette] : '?';
}

struct Point {
  int col;
  int row;
};

constexpr bool pointLess(Point left, Point right) {
  return left.row < right.row ||
         (left.row == right.row && left.col < right.col);
}

inline void sortPoints(Point (&points)[4]) {
  for (int i = 0; i < 3; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      if (pointLess(points[j], points[i])) {
        const Point held = points[i];
        points[i] = points[j];
        points[j] = held;
      }
    }
  }
}

inline void normalizePoints(Point (&points)[4]) {
  int minCol = points[0].col;
  int minRow = points[0].row;
  for (int i = 1; i < 4; ++i) {
    if (points[i].col < minCol) minCol = points[i].col;
    if (points[i].row < minRow) minRow = points[i].row;
  }
  for (Point& point : points) {
    point.col -= minCol;
    point.row -= minRow;
  }
  sortPoints(points);
}

constexpr bool samePoints(const Point (&left)[4], const Point (&right)[4]) {
  for (int i = 0; i < 4; ++i) {
    if (left[i].col != right[i].col || left[i].row != right[i].row) {
      return false;
    }
  }
  return true;
}

inline bool rectCell(ui::Rect rect, int& col, int& row) {
  if (rect.w != ui::kCell || rect.h != ui::kCell) return false;
  const int x = static_cast<int>(rect.x) - ui::kFieldX;
  const int y = static_cast<int>(rect.y) - ui::kFieldY;
  if (x < 0 || y < 0 || x % ui::kCell != 0 || y % ui::kCell != 0) {
    return false;
  }
  col = x / ui::kCell;
  row = y / ui::kCell;
  return col >= 0 && col < ui::kCols && row >= 0 && row < ui::kRows;
}

inline char activeLetter(const ui::PlayingModel& model) {
  Point actual[4]{};
  for (int i = 0; i < 4; ++i) {
    if (!rectCell(model.active[i], actual[i].col, actual[i].row)) return '?';
  }
  normalizePoints(actual);

  for (int piece = 0; piece < kPieceCount; ++piece) {
    const PieceId id = static_cast<PieceId>(piece);
    for (int state = 0; state < 4; ++state) {
      Point candidate[4]{};
      const Shape& shape = shapeOf(id, state);
      for (int i = 0; i < 4; ++i) {
        candidate[i] = Point{shape[static_cast<std::size_t>(i)].col,
                             shape[static_cast<std::size_t>(i)].row};
      }
      normalizePoints(candidate);
      if (samePoints(actual, candidate)) return letterFor(id);
    }
  }
  return '?';
}

inline void menuItem(Writer& writer, bool selected, const char* text) {
  writer.text(selected ? "> " : "  ");
  writer.text(text);
  writer.newline();
}

inline int renderPlaying(Writer& writer, const ui::PlayingModel& model) {
  char field[ui::kRows][ui::kCols]{};
  for (int row = 0; row < ui::kRows; ++row) {
    for (int col = 0; col < ui::kCols; ++col) {
      field[row][col] = paletteLetter(model.field[row][col], '.');
    }
  }

  if (!model.ghostHidden) {
    for (const ui::Rect rect : model.ghost) {
      int col = 0;
      int row = 0;
      if (rectCell(rect, col, row)) field[row][col] = 'o';
    }
  }
  const char active = activeLetter(model);
  for (const ui::Rect rect : model.active) {
    int col = 0;
    int row = 0;
    if (rectCell(rect, col, row)) field[row][col] = active;
  }

  writer.text(model.hud.left);
  writer.text(" | ");
  writer.text(model.hud.mid);
  writer.text(" | ");
  writer.text(model.hud.right);
  writer.newline();

  writer.text("############| HOLD:");
  writer.character(paletteLetter(model.hold, '-'));
  writer.text(" NEXT:");
  for (int i = 0; i < 3; ++i) {
    if (i != 0) writer.character(',');
    writer.character(paletteLetter(model.next[i], '-'));
  }
  writer.newline();
  for (int row = 0; row < ui::kRows; ++row) {
    writer.character('#');
    for (int col = 0; col < ui::kCols; ++col) {
      writer.character(field[row][col]);
    }
    writer.character('#');
    writer.newline();
  }
  writer.text("############");
  writer.newline();

  // The footer is the already-fitted literal selected by buildPlaying().
  writer.text(model.banner);
  writer.newline();
  return writer.finish();
}

}  // namespace ascii_detail

inline int renderAscii(char* out, std::size_t n,
                       const ui::TitleModel& model) {
  ascii_detail::Writer writer(out, n);
  writer.text(model.title);
  writer.newline();
  writer.text("VERSION ");
  writer.text(model.version);
  writer.newline();
  writer.text("MODE ");
  writer.text(model.mode);
  writer.newline();
  writer.text("HIGH SCORE ");
  writer.unsignedValue(model.highScore);
  writer.newline();
  for (uint8_t i = 0; i < 4; ++i) {
    ascii_detail::menuItem(writer, model.menuIndex == i, model.items[i]);
  }
  return writer.finish();
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::PlayingModel& model) {
  ascii_detail::Writer writer(out, n);
  return ascii_detail::renderPlaying(writer, model);
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::PausedModel& model) {
  ascii_detail::Writer writer(out, n);
  ascii_detail::renderPlaying(writer, model.behind);
  writer.text("PAUSED");
  writer.newline();
  for (uint8_t i = 0; i < 4; ++i) {
    ascii_detail::menuItem(writer, model.index == i, model.items[i]);
  }
  return writer.finish();
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::GameOverModel& model) {
  ascii_detail::Writer writer(out, n);
  writer.text("GAME OVER\nSCORE ");
  writer.unsignedValue(model.score);
  writer.text("\nLINES ");
  writer.unsignedValue(model.lines);
  writer.text("\nLEVEL ");
  writer.unsignedValue(model.level);
  writer.text("\nTIME ");
  writer.unsignedValue(model.elapsedMs);
  writer.newline();
  if (model.isHighScore) {
    writer.text("HIGH SCORE RANK ");
    writer.unsignedValue(model.rank);
    writer.newline();
  }
  for (uint8_t i = 0; i < 2; ++i) {
    ascii_detail::menuItem(writer, model.index == i, model.items[i]);
  }
  return writer.finish();
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::HighScoresModel& model) {
  ascii_detail::Writer writer(out, n);
  writer.text("HIGH SCORES");
  writer.newline();
  for (const ui::HighScoresModel::Row& row : model.rows) {
    writer.text(row.text);
    writer.newline();
  }
  return writer.finish();
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::SettingsModel& model) {
  ascii_detail::Writer writer(out, n);
  writer.text("SETTINGS");
  writer.newline();
  for (uint8_t i = 0; i < ui::kSettingsRows; ++i) {
    const ui::SettingsModel::Row& row = model.rows[i];
    writer.text(model.index == i ? "> " : "  ");
    if (row.destructive) writer.text("! ");
    writer.text(row.label);
    if (row.value != nullptr && row.value[0] != '\0') {
      writer.text(": ");
      writer.text(row.value);
    }
    writer.newline();
  }
  return writer.finish();
}

inline int renderAscii(char* out, std::size_t n,
                       const ui::InstructionsModel& model) {
  ascii_detail::Writer writer(out, n);
  writer.text("INSTRUCTIONS");
  writer.newline();
  for (uint8_t i = 0; i < model.count; ++i) {
    writer.text(model.lines[i]);
    writer.newline();
  }
  return writer.finish();
}

}  // namespace sf::test
