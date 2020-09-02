#pragma once
#ifndef CATA_SRC_WINDOWING_H
#define CATA_SRC_WINDOWING_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <functional>
#include <iterator>
#include <locale>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "catacharset.h"
#include "color.h"
#include "debug.h"
#include "enums.h"
#include "item.h"
#include "line.h"
#include "point.h"
#include "string_formatter.h"
#include "translations.h"
#include "units.h"

namespace catacurses
{
class window;

using chtype = int;
} // namespace catacurses

// A simple scrolling view onto some text.  Given a window, it will use the
// leftmost column for the scrollbar and fill the rest with text.  When the
// scrollbar is not needed it prints a vertical border in place of it, so the
// expectation is that the given window will overlap the left edge of a
// bordered window if one exists.
// (Options to e.g. not print the border would be easy to add if needed).
// Update the text with set_text (it will be wrapped for you).
// scroll_up and scroll_down are expected to be called from handlers for the
// keys used for that purpose.
// Call draw when drawing related UI stuff.  draw calls werase/wnoutrefresh for its
// window internally.
class text_pane
{
    public:
        enum cursor_style {
            cursor_hidden,
            cursor_highlighted,
            cursor_bracketed
        };

        enum scrollbar_pos {
            scrollbar_left,
            scrollbar_right,
            scrollbar_none
        };

        enum entry_flags {
            entry_default = 0,
            entry_skipped = ( 1 << 0 ),
            entry_darkened = ( 1 << 1 ),
            entry_highlighted = ( 1 << 2 ),
            entry_bracketed = ( 1 << 3 ),
            entry_hidden = ( 1 << 4 )
        };

        text_pane( catacurses::window &w ) : w_( w ) {
            cursor_color_ = c_white;
        }
        void set_simple_text( const std::string & );
        void set_unwrapped_text( std::vector<std::string> & );

        void set_scrollbar_pos();
        void set_wrap_cursor( bool );
        void set_cursor_hidden();
        void set_cursor_highlighted();
        void set_cursor_bracketed( std::string prefix, std::string suffix,
                                   const nc_color &color = c_white );
        void set_cursor_pos( int pos );
        int get_cursor_pos();
        void cursor_up( size_t lines = 1 );
        void cursor_down( size_t lines = 1 );
        void page_up();
        void page_down();
        void draw( const nc_color &base_color );
    private:
        int text_width();
        int num_lines();
        size_t max_offset();

        struct text_pane_entry {
                bool has_flag( entry_flags this_flag );
                const std::vector<std::string> &get_content();

                void add_text( std::string new_text );
                void set_flag( entry_flags this_flag );
                void clear_flags();
                void clear_text();

                std::vector<std::string> content_;
                size_t folded_line_count_;
                entry_flags flags_;
        };


        catacurses::window &w_;
        std::pair<std::string, std::string> cursor_text_ = { "", "" };
        std::vector<std::string> output_strings_;
        std::vector<text_pane_entry> input_dataset_;
        size_t cursor_pos_ = 0;
        nc_color cursor_color_;
        cursor_style cursor_style_ = cursor_hidden;
        scrollbar_pos scrollbar_pos_ = scrollbar_left;
        bool wrap_cursor_ = false;
        int cursor_offset_ = 0;
};

#endif // CATA_SRC_WINDOWING_H
