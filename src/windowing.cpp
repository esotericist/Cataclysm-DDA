#include "windowing.h"

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <type_traits>
#include <cmath>

#include "cata_utility.h"
#include "catacharset.h"
#include "color.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "input.h"
#include "item.h"
#include "line.h"
#include "name.h"
#include "options.h"
#include "popup.h"
#include "rng.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "output.h"
#include "ui_manager.h"
#include "units.h"
#include "point.h"
#include "wcwidth.h"


bool text_pane::text_pane_entry::has_flag( entry_flags this_flag )
{
    return flags_ & this_flag;
}

const std::vector<std::string> &text_pane::text_pane_entry::get_content()
{
    return content_;
}

/*
size_t text_pane::text_pane_entry::get_line_count()
{
    return folded_line_count_;
}
*/

void text_pane::text_pane_entry::set_flag( entry_flags this_flag )
{
    flags_ = static_cast<entry_flags>( flags_ | this_flag );
}

void text_pane::text_pane_entry::clear_text()
{
    content_.clear();
    folded_line_count_ = 0;
}

void text_pane::text_pane_entry::clear_flags()
{
    flags_ = entry_default;
}

void text_pane::text_pane_entry::add_text( std::string new_text )
{
    content_.emplace_back( new_text );

}

void text_pane::set_simple_text( const std::string &text )
{
    cursor_pos_ = 0;
    input_dataset_.clear();
    output_strings_.clear();
    std::vector<std::string> input_entries = foldstring( text, text_width() );

    text_pane_entry thisentry;
    size_t i = 0;
    for( const auto &k : input_entries ) {
        thisentry.content_.emplace_back( k );
        output_strings_.emplace_back( k );
        if( k == "" ) {
            if( i++ % 4 ) {
                thisentry.flags_ = entry_flags::entry_darkened;
                i = 0;
            }
            thisentry.folded_line_count_ = thisentry.content_.size();
            input_dataset_.emplace_back( thisentry );
            thisentry.content_.clear();
        }
    }
    if( !thisentry.content_.empty() ) {
        input_dataset_.emplace_back( thisentry );
    }


}

void text_pane::cursor_up( size_t lines )
{
    if( cursor_pos_ >= lines ) {
        cursor_pos_ -= lines;
    } else {
        cursor_pos_ = 0;
    }
}

void text_pane::cursor_down( size_t lines )
{
    if( cursor_pos_ + lines < input_dataset_.size() - 1 ) {
        cursor_pos_ += lines;
    } else {
        cursor_pos_ = input_dataset_.size() - 1;
    }
}

void text_pane::page_up()
{
    cursor_offset_ = std::max<size_t>( 0, cursor_offset_ - getmaxy( w_ ) );
}

void text_pane::page_down()
{
    cursor_offset_ = std::min<size_t>( max_offset(), cursor_offset_ + getmaxy( w_ ) );
}


void calc_start_pos( int &iStartPos, const int iCurrentLine, const int iContentHeight,
                     const int iNumEntries, const int entrysize )
{
    if( entrysize ) {

    }
    if( iNumEntries <= iContentHeight ) {
        iStartPos = 0;
    } else if( get_option<bool>( "MENU_SCROLL" ) ) {
        iStartPos = iCurrentLine - ( iContentHeight - 1 ) / 2  + ( entrysize / 2 );
        if( iStartPos < 0 ) {
            iStartPos = 0;
        } else if( iStartPos + iContentHeight > iNumEntries ) {
            iStartPos = iNumEntries -  iContentHeight;
        }
    } else {
        if( iCurrentLine < iStartPos ) {
            iStartPos = iCurrentLine - 2;
        } else if( iCurrentLine >= iStartPos + ( iContentHeight - entrysize ) ) {
            iStartPos =  iCurrentLine - ( iContentHeight - entrysize );
        }
    }
    if( iStartPos < 0 ) {
        iStartPos = 0;
    }
    if( iStartPos > iNumEntries ) {
        iStartPos = iNumEntries;
    }
}

void text_pane::draw( const nc_color &base_color )
{
    werase( w_ );

    const int height = getmaxy( w_ );

    int left_margin = utf8_width( cursor_text_.first, true ) + 1;
    int right_margin = getmaxx( w_ ) - utf8_width( cursor_text_.second, true );
    nc_color cursor_color = cursor_color_;

    std::vector<int> entry_lengths;
    for( const auto &e : input_dataset_ ) {
        entry_lengths.emplace_back( e.folded_line_count_ );
    }
    std::vector<int> entry_offsets = { 0 };
    std::partial_sum( entry_lengths.begin(), entry_lengths.end(), std::back_inserter( entry_offsets ) );

    /*
    int cur_offset = entry_offsets[ cursor_pos_ ] + ( output_dataset_[cursor_pos_].content_.size() /
                     2 );
    */
    size_t cur_offset = entry_offsets[ cursor_pos_ ];
    size_t w_height = getmaxy( w_ );
    calc_start_pos( cursor_offset_, cur_offset, w_height, output_strings_.size(),
                    input_dataset_[cursor_pos_].folded_line_count_ );
    size_t end_offset = cursor_offset_ + w_height;
    if( end_offset > output_strings_.size() - 1 ) {
        end_offset = output_strings_.size() - 1;
    }

    if( max_offset() > 0 ) {
        scrollbar().
        content_size( output_strings_.size() ).
        viewport_pos( cursor_offset_ ).
        viewport_size( height ).
        scroll_to_last( false ).
        apply( w_ );
    } else {
        // No scrollbar; we need to draw the window edge instead
        for( int i = 0; i < height; i++ ) {
            mvwputch( w_, point( 0, i ), BORDER_COLOR, LINE_XOXO );
        }
    }

    /*
    size_t first_entry = std::distance( entry_offsets.begin(), std::upper_bound( entry_offsets.begin(),
                                        entry_offsets.end(), start_offset ) ) - 1;
    size_t last_entry = std::distance( entry_offsets.begin(), std::lower_bound( entry_offsets.begin(),
                                       entry_offsets.end(), end_offset ) ) - 1;


    if( last_entry >= input_dataset_.size() ) {
        last_entry = input_dataset_.size() - 1;
    }

    */

    size_t display_line = 0;
    for( size_t current_line = cursor_offset_; current_line <= end_offset; current_line++ ) {
        if( display_line >= w_height ) {
            continue;
        }
        nc_color color = base_color;

        size_t current_entry = std::distance( entry_offsets.begin(),
                                              std::upper_bound( entry_offsets.begin(), entry_offsets.end(), current_line ) ) - 1;

        text_pane_entry &this_entry = input_dataset_[current_entry];
        bool is_darkened = this_entry.has_flag( entry_darkened );
        bool is_highlighted = this_entry.has_flag( entry_highlighted );
        bool is_current = current_entry == cursor_pos_;
        bool is_cursor_highlight = cursor_style_ == cursor_highlighted;

        if( is_darkened ) {
            color = c_dark_gray;
            if( is_current && is_cursor_highlight ) {
                color = h_dark_gray;
            }
        } else if( is_highlighted ) {
            color = c_yellow;
            if( is_current && is_cursor_highlight ) {
                color = h_yellow;
            }
        } else {
            color = c_white;
            if( is_current && is_cursor_highlight ) {
                color = h_white;
            }
        }
        if( is_current && cursor_style_ == cursor_bracketed ) {
            print_colored_text( w_, point( 1, display_line ), cursor_color, cursor_color_, cursor_text_.first );
            print_colored_text( w_, point( right_margin, display_line ), cursor_color, cursor_color,
                                cursor_text_.second );
        }
        print_colored_text( w_, point( left_margin, display_line ), color, color,
                            output_strings_[current_line] );
        display_line += 1;

    }

    /*
    size_t list_line = 0;
    for( size_t current_entry = first_entry; current_entry <= last_entry; current_entry++ ) {
        nc_color color = base_color;
        if( input_dataset_[current_entry].flags & entry_flags::entry_darkened ) {
            color = c_dark_gray;
            if( current_entry == cursor_pos_ && cursor_style_ == cursor_highlighted ) {
                color = h_dark_gray;
            }
        } else if( input_dataset_[current_entry].flags & entry_flags::entry_highlithed ) {
            color = c_yellow;
            if( current_entry == cursor_pos_ && cursor_style_ == cursor_highlighted ) {
                color = h_yellow;
            }
        } else {
            color = c_white;
            if( current_entry == cursor_pos_ && cursor_style_ == cursor_highlighted ) {
                color = h_white;
            }
        }
        std::vector<std::string> &entry_text = input_dataset_[current_entry].content_;
        size_t start_line = 0;
        size_t end_line = entry_text.empty() ? 0 : entry_text.size() - 1;
        if( current_entry == first_entry ) {
            start_line = entry_offsets[current_entry] - start_offset;
        }
        if( current_entry == ( last_entry ) ) {
            if( entry_offsets[current_entry] < end_offset ) {
                end_line = end_offset - entry_offsets[current_entry];
                if( end_line >= entry_text.size() ) {
                    end_line = entry_text.size() - 1;
                }
            } else {
                end_line = 0;
            }
        }
        for( size_t current_line = start_line; current_line < end_line ; current_line++ ) {
            if( current_entry == cursor_pos_ ) {
                print_colored_text( w_, point( 1, list_line ), cursor_color, cursor_color_, cursor_text_.first );
                print_colored_text( w_, point( right_margin, list_line ), cursor_color, cursor_color,
                                    cursor_text_.second );
            }
            print_colored_text( w_, point( left_margin, list_line ), color, color, entry_text[current_line] );
            list_line += 1;
        }
    }
    */

    wnoutrefresh( w_ );
}

void text_pane::set_cursor_hidden()
{
    cursor_style_ = cursor_hidden;
    cursor_text_ = std::pair<std::string, std::string>( "", "" );
}

void text_pane::set_cursor_highlighted()
{
    cursor_style_ = cursor_highlighted;
    cursor_text_ = std::pair<std::string, std::string>( "", "" );
}

void text_pane::set_cursor_bracketed( std::string prefix, std::string suffix,
                                      const nc_color &color )
{
    cursor_style_ = cursor_bracketed;
    cursor_text_ = std::pair<std::string, std::string>( prefix, suffix );
    cursor_color_ = color;
}

int text_pane::text_width()
{
    int windowwidth = getmaxx( w_ );
    int cursorwidth = utf8_width( cursor_text_.first, true ) + utf8_width( cursor_text_.second, true );
    int adj = scrollbar_pos_ == text_pane::scrollbar_none ? 0 : 1;
    return windowwidth - adj - cursorwidth;
}

int text_pane::num_lines()
{
    return output_strings_.size();
}

size_t text_pane::max_offset()
{
    int linecount = num_lines();
    int maxy = getmaxy( w_ );
    if( linecount || maxy ) {

    }
    return std::max( 0, num_lines() - getmaxy( w_ ) );
}
