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


void text_pane::set_simple_text( const std::string &text )
{
    cursor_pos_ = 0;
    input_entries_ = foldstring( text, text_width() );
    output_dataset_.clear();
    text_pane::entry thisentry;
    size_t i = 0;
    for( const auto &k : input_entries_ ) {
        thisentry.content_.emplace_back( k );
        if( k == "" ) {
            if( i++ % 4 ) {
                thisentry.flags = entry_flags::entry_darkened;
            }
            output_dataset_.emplace_back( thisentry );
            thisentry.content_.clear();
        }
    }
    if( !thisentry.content_.empty() ) {
        output_dataset_.emplace_back( thisentry );
    }
    folded_line_count_ = input_entries_.size();
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
    if( cursor_pos_ + lines < output_dataset_.size() - 1 ) {
        cursor_pos_ += lines;
    } else {
        cursor_pos_ = output_dataset_.size() - 1;
    }
}

void text_pane::page_up()
{
    offset_ = std::max<size_t>( 0, offset_ - getmaxy( w_ ) );
}

void text_pane::page_down()
{
    offset_ = std::min<size_t>( max_offset(), offset_ + getmaxy( w_ ) );
}


int calc_start_pos( const int iCurrentLine, const int iContentHeight,
                    const int iNumEntries )
{
    int iStartPos = 0;
    if( iNumEntries <= iContentHeight ) {
        iStartPos = 0;
    } else if( get_option<bool>( "MENU_SCROLL" ) ) {
        iStartPos = iCurrentLine - ( iContentHeight - 1 ) / 2;
        if( iStartPos < 0 ) {
            iStartPos = 0;
        } else if( iStartPos + iContentHeight > iNumEntries ) {
            iStartPos = iNumEntries - iContentHeight;
        }
    } else {
        if( iCurrentLine < iStartPos ) {
            iStartPos = iCurrentLine;
        } else if( iCurrentLine >= iStartPos + iContentHeight ) {
            iStartPos = 1 + iCurrentLine - iContentHeight;
        }
    }
    return iStartPos;
}

void text_pane::draw( const nc_color &base_color )
{
    werase( w_ );

    const int height = getmaxy( w_ );

    int left_margin = utf8_width( cursor_text_.first, true ) + 1;
    int right_margin = getmaxx( w_ ) - utf8_width( cursor_text_.second, true );
    nc_color cursor_color = cursor_color_;

    std::vector<int> entry_lengths;
    for( const auto &e : output_dataset_ ) {
        entry_lengths.emplace_back( e.content_.size() );
    }
    std::vector<int> entry_offsets = { 0 };
    std::partial_sum( entry_lengths.begin(), entry_lengths.end(), std::back_inserter( entry_offsets ) );

    /*
    int cur_offset = entry_offsets[ cursor_pos_ ] + ( output_dataset_[cursor_pos_].content_.size() /
                     2 );
    */
    int cur_offset = entry_offsets[ cursor_pos_ ];
    size_t w_height = getmaxy( w_ );
    int start_offset = calc_start_pos( cur_offset, w_height, folded_line_count_ );
    int end_offset = start_offset + w_height;

    size_t first_entry = std::distance( entry_offsets.begin(), std::upper_bound( entry_offsets.begin(),
                                        entry_offsets.end(), start_offset ) ) - 1;
    size_t last_entry = std::distance( entry_offsets.begin(), std::lower_bound( entry_offsets.begin(),
                                       entry_offsets.end(), end_offset ) ) - 1;
    if( last_entry >= output_dataset_.size() ) {
        last_entry = output_dataset_.size() - 1;
    }

    if( max_offset() > 0 ) {
        scrollbar().
        content_size( folded_line_count_ ).
        viewport_pos( start_offset ).
        viewport_size( height ).
        scroll_to_last( false ).
        apply( w_ );
    } else {
        // No scrollbar; we need to draw the window edge instead
        for( int i = 0; i < height; i++ ) {
            mvwputch( w_, point( 0, i ), BORDER_COLOR, LINE_XOXO );
        }
    }

    size_t list_line = 0;
    for( size_t current_entry = first_entry; current_entry <= last_entry; current_entry++ ) {
        nc_color color = base_color;
        if( output_dataset_[current_entry].flags & entry_flags::entry_darkened ) {
            color = c_dark_gray;
            if( current_entry == cursor_pos_ && cursor_style_ == cursor_highlighted ) {
                color = h_dark_gray;
            }
        } else if( output_dataset_[current_entry].flags & entry_flags::entry_highlithed ) {
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
        std::vector<std::string> &entry_text = output_dataset_[current_entry].content_;
        size_t start_line = 0;
        size_t end_line = entry_text.empty() ? 0 : entry_text.size() -1;
        if( current_entry == first_entry ) {
            start_line = entry_offsets[current_entry] - start_offset;
        }
        if( current_entry == ( last_entry ) ) {
            if( entry_offsets[current_entry] < end_offset ) {
                end_line = end_offset - entry_offsets[current_entry];
                if( end_line >= entry_text.size() ) {
                    end_line = entry_text.size() -1;
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
    return folded_line_count_;
}

int text_pane::max_offset()
{
    return std::max( 0, num_lines() - getmaxy( w_ ) );
}
