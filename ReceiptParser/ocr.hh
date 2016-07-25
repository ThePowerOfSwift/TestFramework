#ifndef _OCRPARSER_OCR_H__
#define _OCRPARSER_OCR_H__

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <ctype.h>

using namespace std;

#import "common.hh"

namespace ocrparser {
    
#define OCR_ACCEPTABLE_ERROR					0.094
#define OCR_ACCEPTABLE_ERROR_LOWHURDLE			0.15
// Amount in % by which accents on uppercase letters are expected to exceed normal uppercase height
#define OCR_ACCENT_ADDITION     0.21

// Possible data type for SmartPtr requiring a special dealloc
#define wstringE        1
#define vectorwstringE  2
#define vectorintE      3
#define rangeE          4
#define intE            5
#define floatE          6
#define vectorsmartptrmatchE 7
#define smartptrmatchE          8
#define boolE           9
#define imageE          10
#define ocrcolumnE      11
#define ocrlineE        12
#define ocrpageE        13
#define ocrwordE        14
#define ocrrectE           15
#define ocrstatsE       16
#define cgrectE         17
#define vectorsmartptrcgrectE 18
#define sizetE          19
#define ocrelementtypeE 21
#define vectormatchptrE 22
#define ocrresultsE     23
#define vectorgrouptypeE 24

bool isTallLowercase(wchar_t ch);
bool isLetter(wchar_t ch);
std::string toUTF8(const std::wstring& utf32);
std::wstring toUTF32(const std::string& utf8);
void convertIndexes(const std::string& utf8, int byteOffset, int byteLen, int& letterOffset, int& letterCount);
void convertIndexesToBytes(const std::string& utf8, int letterOffset, int letterCount, int& byteOffset, int& byteLen);

int sequence_length(char leadChar);

struct GroupType {
  int groupId;
  OCRElementType type;
  int flags;
  GroupType(int gid ,OCRElementType t, int f = 0) : groupId(gid), type(t), flags(f){}
};

struct Range {
    int location;
    int length;
    Range():location(0),length(0){}
    Range(int loc, int len):location(loc),length(len){}
};

struct EmptyType{};

struct origin_t {
	float x;
	float y;
	origin_t():x(0),y(0){}
	origin_t(float _x, float _y):x(_x),y(_y){}

	bool operator==(const origin_t& l)const{return (x == l.x) && (y == l.y);}
};

struct dimensions {
	float width;
	float height;
	dimensions():width(0), height(0){}
	dimensions(float w, float h):width(w), height(h){}
	bool operator==(const dimensions& l) const {return (width == l.width)
		&& (height == l.height);}
};

struct Rect {
	origin_t origin;
	dimensions size;
	Rect(float x, float y, float width, float height):origin(x,y),
	size(width,height){}

	Rect(){}


	bool operator==(const Rect& l) const {return (origin == l.origin) &&
		(size == l.size);}

};

typedef Rect CGRect;

class Image;
class OCRColumn;
class OCRLine;
class OCRPage;
class OCRWord;
class OCRRect;
class OCRStats;
class OCRResults;

struct MatchPtr;
	
class RC {
private:
	int count; // Reference count
public:
	RC() : count(1) {}
	void AddRef() {
		// Increment the reference
		count++;
        }

	int Release() {
		// Decrement the reference count and
		// return the reference count.
		return --count;
	}
};

template < typename T > class SmartPtr {
private:
	T*    pData;    // pointer

	RC* reference;  // Reference count
    
    int typeID;     // Type of underlying data
public:
	SmartPtr(T* pValue, RC* ref, int tp) : pData(pValue), reference(ref), typeID(tp) {

		reference->AddRef();
	}

public:
	bool isNull() {return pData == NULL;}
	SmartPtr() : pData(0), reference(new RC()), typeID(0) // MM911 source of the leak?
	{ // I know it is stupid to reference count NULL. but it makes the rest
	  // of the code simpler.
	}

    explicit SmartPtr(std::wstring * pValue) : pData(pValue), reference(new RC())
	{
        typeID = wstringE;
    }
    
    explicit SmartPtr(std::vector<std::wstring> * pValue) : pData(pValue), reference(new RC())
	{
        typeID = vectorwstringE;
    }
    
    explicit SmartPtr(std::vector<int> * pValue) : pData(pValue), reference(new RC())
	{
        typeID = vectorintE;
    }
    
    explicit SmartPtr(Range * pValue) : pData(pValue), reference(new RC()), typeID(rangeE)
	{
    }
    
    explicit SmartPtr(int * pValue) : pData(pValue), reference(new RC())
	{
        typeID = intE;
    }
    
    explicit SmartPtr(float * pValue) : pData(pValue), reference(new RC())
	{
        typeID = floatE;
    }
    
    explicit SmartPtr(std::vector<SmartPtr< std::map<std::string, SmartPtr<EmptyType> > > > * pValue) : pData(pValue), reference(new RC()), typeID(vectorsmartptrmatchE)
	{
    }
    
    explicit SmartPtr(std::map<std::string, SmartPtr<EmptyType> > * pValue) : pData(pValue), reference(new RC())
	{
        typeID = smartptrmatchE;
    }
    
    explicit SmartPtr(bool * pValue) : pData(pValue), reference(new RC())
	{
        typeID = boolE;
    }

    explicit SmartPtr(Image * pValue) : pData(pValue), reference(new RC())
	{
        typeID = imageE;
    }
    
    explicit SmartPtr(OCRColumn * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrcolumnE;
    }
    
    explicit SmartPtr(OCRLine * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrlineE;
    }
    
    explicit SmartPtr(OCRPage * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrpageE;
    }
    
    explicit SmartPtr(OCRWord * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrwordE;
    }
    
    explicit SmartPtr(OCRRect * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrrectE;
    }
    
    explicit SmartPtr(OCRStats * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrstatsE;
    }
    
    explicit SmartPtr(CGRect * pValue) : pData(pValue), reference(new RC())
	{
        typeID = cgrectE;
    }
    
    explicit SmartPtr(std::vector<SmartPtr<CGRect> > * pValue) : pData(pValue), reference(new RC())
	{
        typeID = vectorsmartptrcgrectE;
    }
    
    explicit SmartPtr(size_t * pValue) : pData(pValue), reference(new RC())
	{
        typeID = sizetE;
    }
    
    explicit SmartPtr(OCRElementType * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrelementtypeE;
    }
    
    explicit SmartPtr(std::vector<MatchPtr> * pValue) : pData(pValue), reference(new RC())
	{
        typeID = vectormatchptrE;
    }
    
    explicit SmartPtr(OCRResults * pValue) : pData(pValue), reference(new RC())
	{
        typeID = ocrresultsE;
    }
    
    explicit SmartPtr(std::vector<GroupType> * pValue) : pData(pValue), reference(new RC())
	{
        typeID = vectorgrouptypeE;
    }

    
    
// Original code:
//    explicit SmartPtr(T* pValue) : pData(pValue), reference(new RC())
//	{
//	}

	SmartPtr(const SmartPtr<T>& sp) : pData(sp.pData),
	reference(sp.reference), typeID(sp.typeID){
		// Copy constructor
		// Copy the data and reference pointer
		// and increment the reference count
		reference->AddRef();
	}

	T* getPtr(){return pData;}
	const T* getPtr()const{return pData;}

	template<typename M> SmartPtr<M> castDown() { // MM911 don't we need to AddRef here?
		return SmartPtr<M>(reinterpret_cast<M*>(pData),
						   reference, typeID);
	}

	template<typename M> const SmartPtr<M> castDown() const { // MM911 don't we need to AddRef here?
		return SmartPtr<M>(reinterpret_cast<M*>(pData),
						   reference, typeID);
	}

	~SmartPtr() {
		// Destructor
		// Decrement the reference count
		// if reference become zero delete the data
		if (reference->Release() == 0) {
            if (pData != NULL) {
                switch (typeID) {
                    case wstringE:
                        delete (std::wstring *)pData;
                        break;
                    case vectorwstringE:
                        ((std::vector<std::wstring> *)pData)->clear();
                        delete (std::vector<std::wstring> *)pData;
                        break;
                    case vectorintE:
                        ((std::vector<int> *)pData)->clear();
                        delete (std::vector<int> *)pData;
                        break;
                    case rangeE:
                        delete (Range *)pData;
                        break;
                    case intE:
                        delete (int *)pData;
                        break;
                    case floatE:
                        delete (float *)pData;
                        break;
                     case vectorsmartptrmatchE:
                        ((std::vector<SmartPtr< std::map<std::string, SmartPtr<EmptyType> > > > *)pData)->clear();
                        delete (std::vector<SmartPtr< std::map<std::string, SmartPtr<EmptyType> > > > *)pData;
                        break;
                     case smartptrmatchE:
                        delete (std::map<std::string, SmartPtr<EmptyType> > *)pData;
                        break;
                    case boolE:
                        delete (bool *)pData;
                        break;
                    case imageE:
                        delete (Image *)pData;
                        break;
                    case ocrcolumnE:
                        delete (OCRColumn *)pData;
                        break;
                    case ocrlineE:
                        delete (OCRLine *)pData;
                        break;
                    case ocrpageE:
                        delete (OCRPage *)pData;
                        break;
                    case ocrwordE:
                        delete (OCRWord *)pData;
                        break;
                    case ocrrectE:
                        delete (OCRRect *)pData;
                        break;
                    case ocrstatsE:
                        delete (OCRStats *)pData;
                        break;
                    case cgrectE:
                        delete (CGRect *)pData;
                        break;
                    case vectorsmartptrcgrectE:
                        ((std::vector<SmartPtr<CGRect> > *)pData)->clear();
                        delete (std::vector<SmartPtr<CGRect> > *)pData;
                        break;
                    case sizetE:
                        delete (size_t *)pData;
                        break;
                    case ocrelementtypeE:
                        delete (OCRElementType *)pData;
                        break;
                    case vectormatchptrE:
                        ((std::vector<MatchPtr> *)pData)->clear();
                        delete (std::vector<MatchPtr> *)pData;
                        break;
                    case ocrresultsE:
                        delete (OCRResults *)pData;
                        break;
                    case vectorgrouptypeE:
                        ((std::vector<GroupType> *)pData)->clear();
                        delete (std::vector<GroupType> *)pData;
                        break;
                    default:
                        delete pData;
                }
            } // pData != NULL
            delete reference;
		} // if ref == 0
	}

	T& operator* () {
		return *pData;
	}

	T* operator-> () {
		return pData;
	}


	const T& operator* () const {
		return *pData;
	}

	const T* operator-> () const {
		return pData;
	}

	bool operator == (const void* other) {
		//                ASSERT(other == NULL);
		return pData == other;
	}

	bool operator != (const void* other) {
		return !this->operator==(other);
	}

	bool operator != (const SmartPtr<T>& other) {
		return !this->operator==(other.pData);
	}

	bool operator == (const SmartPtr<T>& other) {
		return this->operator==(other.pData);
	}

	void swap(SmartPtr<T>& other) {
		RC* tmpRef = other.reference;
		T* tmpData = other.pData;
        int otherTypeID = other.typeID;
        int thisTypeID = this->typeID;
        
        if (otherTypeID != 0)
            typeID = otherTypeID;
        if (thisTypeID != 0)
            other.typeID = thisTypeID;

		other.reference = reference;
		reference = tmpRef;

		other.pData = pData;
		pData = tmpData;
	}

	SmartPtr<T>& operator = (const SmartPtr<T>& sp)
	{
		// Assignment operator
        // MM911 in case of self assignment should we increment the reference count? Update: apparently never happens.
		if (this != &sp) {
			// Avoid self assignment. This is an optimization only. we need be ready in case it does not work.
            int tmpTypeID = sp.typeID;
			SmartPtr<T> tmpCopy(sp);
            tmpCopy.typeID = sp.typeID;
			swap(tmpCopy);
            if (tmpTypeID != 0)
                this->typeID = tmpTypeID;
		}
//        else {
//            this->typeID = sp.typeID; // MM911 just testing
//        }
		return *this;
	}

	void setNull() {
		*this = SmartPtr<T>((T *)NULL);
	}
};

using ::tolower;
using ::toupper;

std::wstring tolower(const std::wstring& source);

std::wstring toupper(const std::wstring& source);

int findAndReplace(std::wstring& data, const std::wstring& searchString,
				   const std::wstring& replaceString );

int findAndReplaceCaseInsensitive(std::wstring& data, const
								  std::wstring& searchString, const std::wstring& replaceString );

int findAndReplace(std::wstring& data, wchar_t find, wchar_t replace);

int findAndReplace(std::wstring& data, wchar_t find, wchar_t replace,
				   int start, int len);

//template <typename String> void split(const String& source, typename
//									  String::value_type separator, std::vector<String>& result) {
//	typename String::size_type prev_pos = 0, pos = 0;
//	while( (pos = source.find(separator, pos)) != String::npos ) {
//		String substring( source.substr(prev_pos, pos-prev_pos) );
//
//		result.push_back(substring);
//		prev_pos = ++pos;
//	}
//	String substring( source.substr(prev_pos, pos-prev_pos) ); // Last word
//	result.push_back(substring);
//}
//
//void split(const std::wstring& source, wchar_t separator,
//		   std::vector<std::wstring>& result);

size_t findCharacterFromSet(const std::wstring& data, const std::wstring& set);
bool findCharacterFromSet(wchar_t ch, const std::wstring& set);

typedef float CGFloat;
    
typedef struct _AverageWithCount{
    CGFloat average;
    CGFloat sum;
    int count;
    _AverageWithCount():average(0), sum(0),count(0){}
    
    _AverageWithCount(float a, float s, int c):average(a),
    sum(s),count(c){}
    
} AverageWithCount;

typedef std::map<std::string, SmartPtr<EmptyType> > Match;

struct MatchPtr :  SmartPtr<Match> {
	explicit MatchPtr(bool init): SmartPtr<Match>(init ? new Match: 0){}
	explicit  MatchPtr(Match* m=0): SmartPtr<Match>(m){}

   bool isExists(const std::string& key) {
   return ((*this)->count(key) > 0) && (!(**this)[key].isNull());
   }

	SmartPtr<EmptyType>& operator[](const std::string& key){
	    return (**this)[key];}

	bool operator ==(const MatchPtr& l) const {return getPtr() ==
		l.getPtr();}

	bool operator == (const void* other) {
		//                ASSERT(other == NULL);
		return getPtr() == other;
	}

	bool operator != (const void* other) {
		return !this->operator==(other);
	}

#ifdef DEBUG
    void print();
#endif

};
typedef std::vector<MatchPtr> MatchVector;
typedef SmartPtr<MatchVector> MatchVectorPtr;

typedef SmartPtr<ocrparser::Rect> RectPtr;
typedef std::vector<RectPtr> RectVector;
typedef SmartPtr<RectVector> RectVectorPtr;


template <class T> T& getTFromMatch(Match& match, std::string type) {

	return *(match[type].castDown<T>());

}

inline float& getFloatFromMatch(Match& match, std::string type) {

	return getTFromMatch<float>(match, type);

}

inline int& getIntFromMatch(Match& match, std::string type) {
	return getTFromMatch<int>(match, type);
}

inline std::wstring& getStringFromMatch(Match& match, std::string type)
{

	return getTFromMatch<std::wstring>(match, type);

}

inline  float& getFloatFromMatch(MatchPtr& match, std::string type) {

	return getTFromMatch<float>(*match, type);

}

inline int& getIntFromMatch(MatchPtr& match, std::string type) {
	return getTFromMatch<int>(*match, type);
}

inline unsigned long& getUnsignedLongFromMatch(MatchPtr& match, std::string type) {
	return getTFromMatch<unsigned long>(*match, type);
}

inline std::wstring& getStringFromMatch(MatchPtr& match, std::string
										type) {

	return getTFromMatch<std::wstring>(*match, type);

}


inline OCRElementType& getElementTypeFromMatch(Match& match, std::string
											   type) {

	return getTFromMatch<OCRElementType>(match, type);

}

inline OCRElementType& getElementTypeFromMatch(MatchPtr& match,
											   std::string type) {

	return getTFromMatch<OCRElementType>(*match, type);

}

} // namespace ocrparser

#endif
