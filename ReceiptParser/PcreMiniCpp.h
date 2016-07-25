#ifndef _MINI_PCRE_CPP_H__
#define _MINI_PCRE_CPP_H__

#include <string>
#include "pcre.h"

///////////////////////////////////////////////////////////////////////////////
// wxPCRE - Regular expression matching (using the PERL Compatible Regular Expression Library)
// Copyright (C) 2007  Christopher Bess (C. Bess Creation)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//		Regular expression support is provided by the PCRE library package,
//   which is open source software, written by Philip Hazel, and copyright
//			 by the University of Cambridge, England.
//		(PCRE available at ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/)
///////////////////////////////////////////////////////////////////////////////
/* v. 0.3.2 */

typedef struct real_pcre pcre;

// ENUM
#if defined(wxPCRE_ONLY)
enum
{ // these are only wxRegEx surrogates
    // use extended regex syntax
	wxRE_EXTENDED = 0, // not implemented

    // use advanced RE syntax (built-in regex only)
	wxRE_ADVANCED = 0, // not implemented

    // use basic RE syntax
	wxRE_BASIC = 0, // not implemented

    // only check match, don't set back references
	wxRE_NOSUB = 0, // not implemented

    // default flags
	wxRE_DEFAULT = wxRE_EXTENDED,

    // ignore case in match
	wxRE_ICASE = PCRE_CASELESS, // compile time only

    // if not set, treat '\n' as an ordinary character, otherwise it is
    // special: it is not matched by '.' and '^' and '$' always match
    // after/before it regardless of the setting of wxRE_NOT[BE]OL
	wxRE_NEWLINE = PCRE_MULTILINE, // compile time only

    // '^' doesn't match at the start of line
	wxRE_NOTBOL = PCRE_NOTBOL, // match time only

    // '$' doesn't match at the end of line
	wxRE_NOTEOL = PCRE_NOTEOL // match time only
};
#endif
// END

/// Witness the power of REGEX!!
class cppPCRE
{
	// cppPCRE members
	public:
		/**
		 * @summary: Creates an instance using default behavior.
		 */
		cppPCRE(void);

		/**
		 * @summary: Creates an instance with a default behavior and the specified vector size.
		 * @note: The default vector holds MAX_VECTOR_SIZE (9999 matches)
		 */
		cppPCRE( int maxVectorSize );

		/**
		 * @summary: Creates an instance and compiles the specified pattern with the given options.
		 * - arg1: the regular expression pattern to compile
		 * - arg2: the options to pass to pcre_compile
		 * @note: Calls Compile(...), use IsValid() or LastError() to determine if the
		 * pattern was successfully compiled.
		 */
		cppPCRE( const std::string& pattern, int options=PCRE_UTF8);

		~cppPCRE();

		/**
		 * @summary: Compiles the regex pattern for use with the pcre_exec function.
		 * - arg1: the regular expression pattern
		 * - arg2: any compile options to pass to pcre_compile
		 * @returns: True if the regular expression pattern compiled without error.
		 */
		bool Compile( const std::string& pattern, int options=PCRE_UTF8 );

		/**
		 * @returns: the description of the last error encountered.
		 */
		std::string LastError() const;

		/**
		 * @returns: the offset (string position) of the last error encountered.
		 */
		inline int LastErrorOffset() const
		{ return mLastErrorOffset; }

		/**
		 * @summary: Gets the matches starting position in the subject string.
		 * - arg1: the subexpression (0 = the entire match)
		 */
		inline int GetMatchStart( int match=0 ) const
		{ return mVector[match*2]; }

		/**
		 * @summary:Gets the matches ending position in the subject string.
		 * - arg1: the subexpression (0 = the entire match)
		 * @returns: the end offset position of the match
		 */
		inline int GetMatchEnd( int match=0 ) const
		{ return mVector[match*2+1]; }

		/**
		 * @summary:Gets the matches length
		 * - arg1: the subexpression (0 = the entire match)
		 * @returns: the length of the match
		 */
		inline int GetMatchLength( int match=0 ) const
		{ return mVector[match*2+1] - mVector[match*2]; }

		/**
		 * - this function is used to exec the regex
		 * - arg 1: the text to search through
		 * - arg 2: any options you wish to pass to pcre_exec
		 * - arg 3: the offset pcre_exec is going to start from (pattern matching start index in arg1)
		 * - arg 4: not used yet, it will be used to store the error generated by pcre_study
		 * @returns: True if the regex pattern found a match
		 */
		bool RegExMatches( const std::string& text, int options, int startOffset, std::string* err=NULL );

	// wxRegEx-like members
	public:
		/**
		 * @summary: Returns true if the regular expression compiled successfully.
		 */
		inline bool IsValid() const
		{ return mRegex != NULL; }

		/**
		 * @summary: Attempts to match the subject string
		 * - arg1: the subject string
		 * - arg2: any match options you want to pass to pcre_exec (o = default options)
		 * @returns: True if the regular expression matched the subject string
		 * @note: Only works AFTER a successful call to Compile(...)
		 */
		inline bool Matches( const std::string& text, int options=0 )
		{ return RegExMatches(text, options, 0, NULL); }

		/**
		 * @summary: Gets the start and length of the match.
		 * - arg1: pointer to start position of the match
		 * - arg2: pointer to length of the match
		 * @returns: True if the match was successful
		 * @note: Only works AFTER a successful call to Compile(...)
		 */
		bool GetMatch( size_t *start, size_t *len, size_t index=0 ) const;

		/**
		 * @summary: Gets the matched string.
		 * - arg1: the subject string
		 * - arg2: the subexpression (0 = the entire match)
		 * @return: a std::string instance of the matched pattern
		 */
		std::string GetMatch( const std::string& text, size_t index=0 ) const;

		/**
		 * @summary: Gets the number of subexpression in the pattern
		 * @returns: the number of subexpressions
		 */
		size_t GetMatchCount() const;

		/**
		 * @summary: Replaces the occurences of the regular expression match, not to exceed
		 * the maxReplacement value.
		 * - arg1: the text to alter (subject string)
		 * - arg2: the replacement text (backreferences are not supported, yet)
		 * - arg3: the replacement limit (0 causes all occurences to be replaced)
		 * @returns: the number of replacements performed
		 */
		int Replace( std::string * text, const std::string& replacement, size_t maxReplacements=0 );

		/**
		 * @summary: Replaces the first occurence of the regular expression match
		 * - arg1: the text to alter (subject string)
		 * - arg2: the replacement text (backreferences are not supported, yet)
		 * @returns: the number of replacements performed
		 */
		inline int ReplaceFirst( std::string * text, const std::string& replacement )
		{ return Replace(text, replacement, 1); }

		/**
		 * @summary: Replaces all occurences of the regular expression match
		 * - arg1: the text to alter (subject string)
		 * - arg2: the replacement text (backreferences are not supported, yet)
		 * @returns: the number of replacements performed
		 */
		inline int ReplaceAll( std::string * text, const std::string& replacement )
		{ return Replace(text, replacement, 0); }

	private:
		pcre * mRegex;
		char * mLastError;
		int mLastErrorOffset;
		int mMaxVectorSize;
		int * mVector;
		int mVectorCount; // passed to pcre_exec (used to create mVector)
		int mExecOptions; // the pcre_exec options
		int mCompileOptions; // the pcre_compile options

		/**
		 * - the return value from the pcre_exec call via cppPCRE::Matches(...)
		 * - the number of matches obtained by pcre_exec
		 */
		int mMatchCount;

	private:
		/**
		 * Common initialize functions
		 */
		void Initialize( int maxVectorSize );
		void Reinitialize();

		// no copies allowed
		cppPCRE( const cppPCRE& );
		cppPCRE &operator=( const cppPCRE& );
};
#endif // _MINI_PCRE_CPP_H__
