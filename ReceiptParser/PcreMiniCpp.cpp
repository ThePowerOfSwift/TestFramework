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

#include <string>
#include <vector>
#include "PcreMiniCpp.h"

#define COMPILE_REGEX_ERROR "Must successfully Compile(...) first"


#define CHECK_MSG(cond, value, msg) if(!(cond)) return value;

using namespace std;

const int MAX_VECTOR_SIZE = 9999;

cppPCRE::cppPCRE()
{
	Initialize(MAX_VECTOR_SIZE);
}

cppPCRE::cppPCRE( int maxVectorSize )
{
	// overwrite the default value
	Initialize(maxVectorSize);
}

cppPCRE::cppPCRE( const std::string& pattern, int options )
{
	Initialize(MAX_VECTOR_SIZE);
    
    options |= PCRE_UTF8;
    options |= PCRE_UCP;

	// compile the regex for the client
	(void)Compile(pattern, options);
}

cppPCRE::~cppPCRE()
{
	if ( mRegex )
		pcre_free(mRegex);

	mRegex = (pcre*)NULL;

	delete [] mVector;
}

void cppPCRE::Initialize( int maxVectorSize )
{
	mRegex = (pcre*)NULL;
	mLastError = (char*)NULL;
	mLastErrorOffset = -1;
	mMaxVectorSize = maxVectorSize;

	// calculate vector info
	int nVectorSize = mMaxVectorSize; // vector will hold mMaxVectoSize matches (start and end index)
	mVectorCount = (1 + nVectorSize) * 3; // should always be a multiple of 3
	mVector = new int[mVectorCount];
}

void cppPCRE::Reinitialize()
{
	if ( IsValid() )
	{
		// cleanup pcre object
		pcre_free(mRegex);
		mRegex = (pcre*)NULL;

		if ( mLastError )
		{
			delete [] mLastError;
			mLastError = (char*)NULL;
		} // end IF

		mLastErrorOffset = -1;
	} // end
}

bool cppPCRE::Compile( const std::string& regexPattern, int options )
{
	// re-init this object
	Reinitialize();

	// convert the std::string to a PCRE safe std::string object
	const char* pattern = (char*)NULL;
	const char* error = (char*)NULL;

    pattern = (const char*)regexPattern.c_str();

    options |= PCRE_UTF8;
    options |= PCRE_UCP;
    
	// store the options
	mCompileOptions = options;

	// perform the pcre regex pattern compilation
	mRegex = pcre_compile(pattern,
					   options,
					   &error, &mLastErrorOffset,
					   NULL);

	// store the error
	mLastError = const_cast<char*>(error);

	// check the regex object (if false then check mLastError)
	bool isValid = this->IsValid();

	// if not valid then reset respective vars
	if ( !isValid )
	{
		mMatchCount = 0;
	} // end IF

	return isValid;
}

bool cppPCRE::RegExMatches( const std::string& text, int options, int startOffset, std::string* error )
{
	// output useful debug info
	CHECK_MSG( IsValid(), false, _(COMPILE_REGEX_ERROR) );

	// store the options
	mExecOptions = options;

	/*
	 - implement later
	// returned value may be used to speed up match
	pcre_extra *regexExtra = pcre_study(mRegex,
										options, // options
										&error);
	*/

	/*
	// get full info
	int pcre_fullinfo(const pcre *code, const pcre_extra *extra,
	int what, void *where);
	int rc;
	size_t len;
	rc = pcre_fullinfo(mRegex, regexExtra, PCRE_INFO_CAPTURECOUNT, &len);

	*/

	// convert the std::string to a std::string (for use with PCRE)
	const char* subject = (char*)NULL;

#if wxUSE_UNICODE
		subject = string(text.mb_str(wxConvUTF8)).c_str();
#else
		subject = (const char*)text.c_str();
#endif

	/*
	int pcre_exec(const pcre *code, const pcre_extra *extra,
	const char *subject, int length, int startoffset,
	int options, int *ovector, int ovecsize);
	*/
	// execute the pattern matching mechanism (obtain match vector, etc)
	mMatchCount = pcre_exec(
	mRegex,             /* result of pcre_compile() */
	0,           /* we didn't study the pattern */
	subject,  /* the subject string */
	(int)text.length(),             /* the length of the subject string */
	startOffset,              /* start at offset ? in the subject */
	mExecOptions,              /* options */
	mVector,        /* vector of integers for substring information */
	mVectorCount);            /* number of elements (NOT size in bytes) */

	return (mVector[0] >= 0);
}

std::string cppPCRE::GetMatch( const std::string& text, size_t match ) const
{
	CHECK_MSG( IsValid(), "", _(COMPILE_REGEX_ERROR) );

	// cast for safety
	int m = static_cast<int>(match);

	/* alternative method
	// get the matched string
	int pcre_get_substring(const char *subject, int *ovector,
	int stringcount, int stringnumber,
	const char **stringptr);
	// returns the length of the string (or the error code if return negative int)
	*/

	// get the std::string value of the match
	return text.substr(GetMatchStart(m), GetMatchLength(m));
}

bool cppPCRE::GetMatch( size_t* start, size_t* len, size_t match ) const
{
	CHECK_MSG( IsValid(), false, _(COMPILE_REGEX_ERROR) );

	// store the length for later use
	size_t l = (size_t)GetMatchLength(match);

	// pass the values to ptrs
	*start = (size_t)GetMatchStart(match);
	*len = l;

	// if the length is useful, match is valid
	return (l >= 1);
}

size_t cppPCRE::GetMatchCount() const
{
	CHECK_MSG( IsValid(), 0, _(COMPILE_REGEX_ERROR) );

	/* - if the match count is less than 0
	 * then it contains the error code, but to the
	 * client of this function its a zero match count
	 */
	if ( mMatchCount < 0 )
		return 0;

	return mMatchCount;
}


#ifdef LATER
int cppPCRE::Replace( std::string* text, const std::string& replacement, size_t max )
{
	CHECK_MSG( IsValid(), false, _(COMPILE_REGEX_ERROR) );

	// check validaty of the regex
	if ( !IsValid() )
		return false;


	// store the current values
	int prevMatchCount = mMatchCount;
	int *prevVector = mVector;

	/*
	 * - first: get all the backreferences (their start/end index and the std::string value)
	 * - second: inject them in the correct pos of the replacement std::string
	 * - final: perform the replacements on all the matches in text (do not replace more that max)
	 */

	size_t startIndex = 0; // what match to begin with
	int nOffset = startIndex;
	size_t nMatch = 0;
	size_t nCount = 0;

	/*
	 * - only match \xx if it is not escaped
	 * - (?<!\\\\)(\\\\)([0-9]+) - will not capture the non-escape char
	 * - ([^\\\\]|^)(\\\\)([0-9]+) - will capture the non-escaping char
	 * - also it seems that you have to escape the escape char for C++ (compiler) and for PCRE (ex: \\\\ = \\ [pcre escape])
	 */
	cppPCRE refRegex("([^\\\\]|^)(\\\\)(\\d+)");

	// do we process back references \xx (are there any backrefs in the replacement string)
	bool obtainRefs = refRegex.Matches(replacement);
	std::vector<std::string>* intArr = NULL;

	if ( obtainRefs )
	{ // start backref grab SCOPE

		intArr = new std::vector<std::string>();
		int nOffset = 0;

		// get all the values of the backrefs
		while ( refRegex.RegExMatches(replacement, 0, nOffset) )
		{
			// get the int value (of the back ref)
			intArr->push_back(refRegex.GetMatch(replacement, 3));

			// move the marker forward
			nOffset = refRegex.GetMatchEnd();

			if ( nOffset == 0 )
				break;
		} // end WHILE
	} // end IF

	// iterate through all the possible matches (or till max)
	while ( RegExMatches(*text, mExecOptions, nOffset) )
	{
		// get the match info
		int start = GetMatchStart(nMatch);
		int len = GetMatchLength(nMatch);

		if ( obtainRefs )
		{
			// the value of the match
			std::string matchStr = text->substr(start, len);

			int limit = intArr->size();
			int i = 0;
			// iterate through all the backrefs
			for ( i = 0 ; i < limit ; ++i )
			{
				/*
				 * - rerun the match using the actual matched string
				 * - this ensures that the replace will have
				 * the correct text pos even after the first replace
				 * - it is faster to convert a string to a number that it is
				 * to convert a number to a string (using std::string::Format)
				 */
				RegExMatches(*text, mExecOptions, nOffset);

				// convert the string to a long
				long matchIdx = 0;
				std::string backRef = (*intArr)[i];
				matchIdx = atoi(backRef.c_str()); // backRef.ToLong(&matchIdx);

				// get the match info
				int s = GetMatchStart(matchIdx);
				int l = GetMatchLength(matchIdx);

				// get the backref string value
				std::string value = text->substr(s, l);

				// setup the replacement text vars
				std::string replacementText = replacement;
				 backRef = "\\" + backRef;

				/*
				 * - replace the backref in 'replacement' with
				 * the value acquired from the actual backref
				 */
				replacementText.Replace(backRef, value);

				// replace the string in the current pos with the actual backref value
				text->replace(start, len, replacementText);
			} // end FOR
		} // end IF (obtainRefs)

		if ( !obtainRefs )
		{
			// replace the matched string the current match
			text->replace(start, len, replacement.c_str());
		} // end IF

		// set next mark past this match
		nOffset = start+len;

		// increment the iteration count
		++nCount;

		// if not max set then keep going
		if ( max != 0 )
		{
			// if we have reached the max
			if ( max == nCount )
				break;
		} // end IF
	} // end WHILE

	// free mem
	if ( intArr )
	{
		intArr->clear();
		delete intArr;
	} // end IF

	// restore to class vars
	mVector = prevVector;
	mMatchCount = prevMatchCount;

	return static_cast<int>(nCount);
}
#endif // LATER

std::string cppPCRE::LastError() const
{
	return mLastError;
}

/* inline functions:
 * ReplaceAll(...)
 * ReplaceFirst(...)
 * IsValid()
 * GetMatchStart(int)
 * GetMatchEnd(int)
 * GetMatchLength(int)
 * LastErrorOffset()
 * Matches(...)
 */
