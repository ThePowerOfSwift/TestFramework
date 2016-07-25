#define CODE_CPP 1

#include "cimage.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <numeric>
#include <string.h>
#include "../common.hh"

cimage::cimage(ubyte* large_image, int large_height, int large_width, Rect rect, bool neighbors4)
{
    m_neighbors4 = neighbors4;
    m_cached_big_component = NULL;
    m_integrated = NULL;
	m_height = rect.size.height + 2;
	m_width =  rect.size.width  + 2;
	m_buffer = new ubyte[m_height * m_width];
	memset(m_buffer, 0, m_height * m_width);
	for (int y = 0; y < rect.size.height; ++y) {
		for (int x = 0; x < rect.size.width; ++x) {
			// (x+1) and (y+1) for borders
			//ubyte bbb = large_image[((y + (int)rect.origin.y) * large_width) + (x + (int)rect.origin.x)];
			//if ((x != 0) && (x != 255)) {
			//	cout << "X = " << hex << (int)bbb << ", " << (int)((ubyte)(bbb > 128)) << endl;
			//}
			//m_buffer[((y+1) * m_width) + (x+1)] = large_image[((y + (int)rect.origin.y) * large_width) + (x + (int)rect.origin.x)] / 255;
			m_buffer[((y+1) * m_width) + (x+1)] = (large_image[((y + (int)rect.origin.y) * large_width) + (x + (int)rect.origin.x)] > 128);
            //m_buffer[((y+1) * m_width) + (x+1)] = large_image[((y + (int)rect.origin.y) * large_width) + (x + (int)rect.origin.x)];
			//m_buffer[((rectHeight - (y+1) - 1) * rectWidth) + (x+1)] = grayScaleImageData[((y + rect.origin.y) * large_width) + (x + rect.origin.x)];
		}
	}
}

//constructor of a zero image by dimensions
cimage::cimage(int height, int width, bool neighbors4) {
    m_neighbors4 = neighbors4;
    m_cached_big_component = NULL;
    m_integrated = NULL;
	m_width = width;
	m_height = height;
	m_buffer = new ubyte[height*width];
	memset(m_buffer,0,height*width);
}

cimage::~cimage() {
	delete[] m_buffer;
	delete m_cached_big_component;
    delete m_integrated;
}

ubyte cimage::getat(int h, int w) {
/*	if ((h>=m_height) || (w>=m_width) || (((h*m_width)+w)>=m_width*m_height)){
		cout << "get out of range:" << h << "," << w << endl;
	}*/
	return m_buffer[(h*m_width) + w];
}
void cimage::setat(int h, int w, ubyte val) {
/*	if ((h>=m_height) || (w>=m_width) || (((h*m_width)+w)>=m_width*m_height)){
		cout << "set out of range:" << h << "," << w << endl;
	}
	cout << "set at:" << h << "," << w << "=" << (h*m_width) + w << endl;*/
	m_buffer[(h*m_width) + w] = val;
}

bool SortPredicate(const CCTYPE d1, CCTYPE d2) {
	return d2[4] < d1[4];
}

CCTYPE_LIST cimage::analyzecomp(bool include_frame, int letter_size, float minSize) {
	//create a buffer for output image
	cimage out(m_height, m_width, m_neighbors4);
	int highest_region = 0;

	for(int h=0; h<m_height; h++) {
		for(int  w=0; w<m_width; w++) {
			if (getat(h,w)==0) {
				continue;
			}
			if ( ((w==0) || (getat(h,w-1)==0)) && ((h==0) || (getat(h-1,w)==0)) && 
                 (m_neighbors4 || (((w==0) || (h==0) || (getat(h-1,w-1)==0)) && ((h==0) || (w==m_width-1) || (getat(h-1,w+1)==0)))) ) {
				out.setat(h,w, highest_region + 1);
				highest_region ++;
				continue;
            }
			if  ( ((w>0) && (getat(h,w-1)>0)) || ((!m_neighbors4) && h>0 && w>0 && getat(h-1,w-1)>0) )  {
				out.setat(h,w, out.getat(h,w-1));

                //this happens if the second condition above is true (8-neighbors)
                //if both conditions are true is has to be the same color anyway
                if (out.getat(h,w)==0) { 
                    out.setat(h,w,out.getat(h-1,w-1));
                }

				if ( ((h>0) && (getat(h-1,w)>0) && (out.getat(h-1,w) != out.getat(h,w))) ||
				     (!m_neighbors4 && h>0 && w<m_width-1 && (getat(h-1,w+1)>0) && (out.getat(h-1,w+1) != out.getat(h,w))) ) {
					ubyte color_to_remove = out.getat(h-1,w);

                    //this happens if the second condition above is true (8 neighbors)
                    //if both conditions are true it has to be the same color anyway...
                    if (color_to_remove == 0) {
					    color_to_remove = out.getat(h-1,w+1);
                    }
					ubyte new_color = out.getat(h,w);
					//remove color and decrease indexes in the image
					for(int h1=0; h1<m_height; h1++) {
						for(int w1=0; w1<m_width; w1++) {
							if (out.getat(h1,w1) == color_to_remove) {
								out.setat(h1,w1, new_color);
							}
							if (out.getat(h1,w1) > color_to_remove) {
								out.setat(h1,w1, out.getat(h1,w1) - 1);
							}
                            if (h1 == h && w1==w) {
                                break;
                            }
						}
                        if (h1 == h) {
                            break;
                        }
					}

					highest_region --;
				}
			} else if ((h>0) && (getat(h-1,w)>0)) {
				out.setat(h,w, out.getat(h-1,w));
			} else if (!m_neighbors4 && h>0 && w<m_width-1 && (getat(h-1,w+1)>0)) {
				out.setat(h,w, out.getat(h-1,w+1));
            }
		}
	}

	float initvals[] = {(float)m_height, (float)m_width, 0, 0, 0};
	CCTYPE initvals_vec(initvals, initvals + (sizeof(initvals) / sizeof(float)));
	CCTYPE_LIST  results(highest_region+1, initvals_vec);

	//frame_tag - connected component to ignore
	int frame_tag;
	if (!include_frame) {
		frame_tag = out.getat(0,0);
	} else {
		frame_tag = 0;
	}

	for(int h=0; h<m_height; h++) {
		for(int w=0; w<m_width; w++) {

			if ((out.getat(h,w) == 0) || (out.getat(h,w) == frame_tag)) {
				continue;
			}
			if (h < results[0][0]) results[0][0] = h;
			if (w < results[0][1]) results[0][1] = w;
			if (h > results[0][2]) results[0][2] = h;
			if (w > results[0][3]) results[0][3] = w;
			results[0][4] += 1;
			int curval = out.getat(h,w);
			if (h < results[curval][0]) results[curval][0] = h;
			if (w < results[curval][1]) results[curval][1] = w;
			if (h > results[curval][2]) results[curval][2] = h;
			if (w > results[curval][3]) results[curval][3] = w;
			results[curval][4] += 1;
		}
	}

    //save the largest connected component cached
    //find the maximal component
    int maxval = 0, maxidx = 0;
    for (unsigned int i=1; i<results.size(); i++) {
        if (results[i][4] > maxval) {
            maxval = results[i][4];
            maxidx = i;
        }
    }
    
    if (m_cached_big_component == NULL) {
        m_cached_big_component = new cimage(m_height, m_width, m_neighbors4);
        for(int h=0; h<m_height; h++) {
            for(int w=0; w<m_width; w++) {
                if (out.getat(h,w) == maxidx) {
                    m_cached_big_component->setat(h,w,1);
                }
            }
        }
    }

	//final_results = []
	if (letter_size == 0) {
        letter_size = (m_height - 2) * (m_width - 2);
	}

	for(int i=(int)results.size()-1; i>0; i--) { //skip the first element
        int currentWidth = (results[i][X_MAX_COORD] - results[i][X_MIN_COORD] + 1);
        int currentHeight = (results[i][Y_MAX_COORD] - results[i][Y_MIN_COORD] + 1);
        float currentSize = currentWidth * currentHeight;
            // Still test area (but again 50% of hurdle) because otherwise I get bogus components with area 0
        if ((results[i][4] < minSize*float(letter_size)*0.50)
            || (currentSize < minSize*float(letter_size)*0.50)
            || ((currentSize < minSize*float(letter_size)) && (currentWidth < (m_width - 2) * 0.20) && (currentHeight < (m_height - 2) * 0.20))
            ) 
        { 
            //cout << "!!!!erased: " << letter_size << ", part:" << results[i][4] << "idx:" << i << endl;
			results.erase(results.begin()+i);
		}
	}

	sort(results.begin(), results.end(), SortPredicate);

	return results;
}

cimage::cimage(char filename[], bool neighbors4) {
    m_neighbors4 = neighbors4;
    m_cached_big_component = NULL;
    m_integrated = NULL;
	fstream filestr (filename, fstream::in);
    if (! filestr.is_open()) {
        m_buffer = NULL;
        return;
    }

	int i;
	char ch;
	int width = 0;
	int count = 0;
	vector<ubyte> buffer;
	buffer.push_back(0);
	while (filestr.good()) {
		filestr >> i;
		if (!filestr.eof()) {
			buffer.push_back(ubyte(i > 128));
			count ++;
		}
		filestr >> ch;
		if (filestr.peek() == '\n') {
			if (width == 0) {
				width = count;
			}
			buffer.push_back(0);
			buffer.push_back(0);
		}
	}
	filestr.close();
	//get rid of the last padding zero
	buffer.erase(buffer.end()-1);

	//allocate image
	m_height = (count/width)+2;
	m_width =  width+2;
	m_buffer = new ubyte[m_height*m_width];

	//add two lines of zeros to the buffer
	buffer.insert(buffer.begin(), m_width, 0);
	buffer.insert(buffer.end()-1, m_width, 0);

	//copy buffer to image
	/*int cnt = 0;
	for(vector<ubyte>::iterator it = buffer.begin(); it != buffer.end(); it++, cnt++) {
		m_buffer[cnt] = *it;
	}*/
	int cnt = 0;
	for (int h=m_height-1; h>=0; h--) {
		for(int  w=0; w<m_width; w++) {
			m_buffer[cnt++] = buffer[(h*m_width) + w];
		}
	}
}

#if ANDROID
void cimage::debug_print() {
    string line;
	//cout << "  ";
    line += " ";
	for (int w=0; w<m_width; w++) {
		if (w<16) {
			//cout << hex << w;
            char *tmpCStr = (char *)malloc(10);
            sprintf(tmpCStr, "%x", w);
            line += tmpCStr;
            free(tmpCStr);
		}
		else {
            //cout << "#";
            line += "#";
        };
	}
	//cout <<endl << "  ";
    DebugLog("%s", line.c_str());
    line = string();
    line += " ";
	for (int w=0; w<m_width; w++) {
        //cout << "-";
        line += "-";
    }
	//cout <<endl;
    DebugLog("%s", line.c_str());
    line = string();
	for(int h=0; h<m_height; h++) {
		if (h<16) {
            //cout << hex << h;
            char *tmpCStr = (char *)malloc(10);
            sprintf(tmpCStr, "%x|", h);
            line += tmpCStr;
            free(tmpCStr);
        } else {
            //cout << "#";
            line += "#|";
        }
		//cout << "|";
		for(int  w=0; w<m_width; w++) {
			int cur = getat(h,w);
			if (cur == 0) {
				//cout << " ";
                line += " ";
			} else if ((cur > 0) && (cur < 10)) {
				//cout << cur;
                line += "1";
			} else {
				//cout << "#";
                line += "#";
			}
		}
		//cout << "|" <<endl;
        line += "|";
        DebugLog("%s", line.c_str());
        line = string();
	}
	//cout << "  ";
    line += "  ";
	for (int w=0; w<m_width; w++) {
        //cout << "-";
        line += "-";
    }
	//cout <<endl << "  ";
    DebugLog("%s", line.c_str());
    line = string();
    line += "  ";
	for (int w=0; w<m_width; w++) {
		if (w<16) {
			//cout << hex << w;
            char *tmpCStr = (char *)malloc(10);
            sprintf(tmpCStr, "%x", w);
            line += tmpCStr;
            free(tmpCStr);
		}
		else {
            //cout << "#";
            line += "#";
        };
	}
	//cout <<endl;
    DebugLog("%s", line.c_str());
}
#else
void cimage::debug_print() {
	cout << "  ";
	for (int w=0; w<m_width; w++) {
		if (w<16) {
			cout << hex << w;
		}
		else { cout << "#"; };
	}
	cout <<endl << "  ";
	for (int w=0; w<m_width; w++) cout << "-";
	cout <<endl;
	for(int h=0; h<m_height; h++) {
		if (h<16) {cout << hex << h;} else { cout << "#"; }
		cout << "|";
		for(int  w=0; w<m_width; w++) {
			int cur = getat(h,w);
			if (cur == 0) {
				cout << " ";
			} else if ((cur > 0) && (cur < 10)) {
				cout << cur;
			} else {
				cout << "#";
			}
		}
		cout << "|" <<endl;
	}
	cout << "  ";
	for (int w=0; w<m_width; w++) cout << "-";
	cout <<endl << "  ";
	for (int w=0; w<m_width; w++) {
		if (w<16) {
			cout << hex << w;
		}
		else { cout << "#"; };
	}
	cout <<endl;
}
#endif // !ANDROID

SegmentList cimage::linetest_vertical(float linecenter, float linewidth) {
    // 2016-04-22 switching from floor to round: doesn't make sense to use 0 when math yields 0.9, does it?
	int startw = 1 + round((m_width - 2)*(linecenter-(linewidth/2))/100);
    if (startw >= m_width)
        startw = m_width - 1;
    // 2016-04-22 switching from ceil to round: doesn't make sense to use 1 when math yields 0.2, does it?
    int endw   = 1 + round((m_width - 2)*(linecenter+(linewidth/2))/100);
    if (endw >= m_width)
        endw = m_width - 1;
    // Make sure we test a line of at least one pixel
    // 03-04-2013 endw is included in the range so start=end is OK, no need to add 1 pixel
    if (endw <= startw) {
        // If we are closer to left, get closer by subs 1 from left range, otherwise start 1 more to the right
        if ((startw - 1) < ((m_width - 1)/2)) {
            endw = startw + 1;
        } else {
            startw = endw - 1;
        }
        // Re-check for validity, just in case
        if (startw >= m_width)
            startw = m_width - 1;
        if (endw >= m_width)
            endw = m_width - 1;
        if (startw < 1)
            startw = 1;
        if (endw < 1)
            endw = 1;
    }
    
    return linetest_vertical_absolute(startw, endw);
}


SegmentList cimage::linetest_vertical_absolute(int startw, int endw) {
        
#if DEBUG_SINGLELETTER
    cout <<"linetest: " << startw << "," << endw << endl;
#endif
	int starth = 0;
	int endh   = m_height;

    int part_start = -1;
	bool last_state = false;
    
    SegmentList results;
    
	for(int h=starth; h<endh; h++) {
		int count = 0;
		for(int w=startw; w<endw; w++) {
			if (getat(h,w) > 0) {
				count ++;
                break;
			}
            if (count > 0)
                break;
		}
        
		if (count > 0) {
            if (!last_state) {
                // start of part
                part_start = h;
                last_state = true;
            }
		} else {
            if (last_state) {
                // end of part
                results.push_back(Segment(part_start, h-1));
                last_state = false;
            }
        }
	}

    // segment till the end
    if (last_state) {
        results.push_back(Segment(part_start, endh-1));
    }

	return results;
}

// Looks for the widest horizontal bar at the top of the letter
bool cimage::top_horiz_bar_test() {
	int starth = 1; 
	int endh = 1 + round((m_height - 2)*0.10);
	int startw = 1;
	int endw   = 1 + (m_width - 2);
    
	int maxCount = 0;
    // Require bar at least 90% of total width
    int countRequired = floor((m_width - 2)* 0.90);
	
    // First step: find the left side of the top line, anywhere within the top 10% of the rect
    for (int x=startw; x<endw; x++) {
        for (int y=starth; y<endh; y++) {
            int count = 0;
            
            // Test what we can do with this as start point for the hor bar
            if (getat(y,x) > 0) {
                // Now see how wide a line we can get starting here
                int lx=x+1; int ly=y;
                count = 1;
                while (lx < endw) {
                    if (getat(ly,lx) > 0) {
                        count++;
                        lx++; continue;
                    } else if ((ly>0) && (getat(ly-1,lx) > 0)) {
                        count++;
                        ly--;
                        lx++; continue;
                    } else if ((ly<(endh-1)) && (getat(ly+1,lx) > 0)) {
                        count++;
                        ly++;
                        lx++; continue;
                    }
                    // Failed to find another dot - abort
                    break;
                }
            }
            
            // See what we got
            if (count > maxCount) {
                if (count >= countRequired)
                    return true;
                else 
                    maxCount = count;
            }
            
            if (count > 0) {
                // We found a pixel at current y, no need to continue down the y axis
                continue;
            }
        }
    }
    return false;
}

SegmentList cimage::linetest_horizontal(float linecenter, float linewidth) {
    // m_height is actual height + 2 (because of dummy white margin of one pixel above and below)
    // 2016-04-22 switching from floor to round: doesn't make sense to use 0 when math yields 0.9, does it?
	int starth = 1 + round((m_height - 2)*(linecenter-(linewidth/2))/100);
    // 2016-04-22 line below was commented out, no idea why. Seems sensible not to fail the call in overflow cases.
    if (starth <= 0) starth = 1;
    if (starth >= m_height) starth = m_height - 1;
    // 2016-04-22 switching from ceil to round: doesn't make sense to use 1 when math yields 0.2, does it?
	int endh   =  1 + round((m_height - 2)*(linecenter+(linewidth/2))/100);
    if (endh > m_height - 1) endh = m_height - 1;
    // Make sure we test a line of at least one pixel
    if (endh <= starth) {
        // If we are closer to bottom, get closer by subs 1 to bottom range, otherwise start 1 lower
        if ((starth - 1) < ((m_height - 1)/2)) {
            endh = starth + 1;
        } else {
            starth = endh - 1;
        }
        // Re-check for validity, just in case
        if (starth >= m_height)
            starth = m_height - 1;
        if (endh >= m_height)
            endh = m_height - 1;
        if (starth < 1)
            starth = 1;
        if (endh < 1)
            endh = 1;
    }

    
    return linetest_horizontal_absolute(starth, endh);
}

// Finds the largest vertical slice with the given number of segments, then returns the box around the specified area. If the caller wants to then get say the 2nd segment out of 3, he needs to call the horizontal segments API to get the segments list then extract the 2nd segment
ocrparser::CGRect cimage::linetest_BoxForNHorizontalSegments_absolute(int nSegments, int starth, int endh, SegmentList &segments) {

    ocrparser::CGRect box(0,0,0,0);
    
    if ((starth >= m_height) || (endh >= m_height) || (starth <= 0) || (endh <= 0))
        return box;
        
    int startw = 0;
	int endw   = m_width;
    

    // Go from starth towards endh until we find a line with the exact number of horizontal segments
    SegmentList results;
    int startMatchingRow = -1, endMatchingRow = -1;
    
    for (int h=starth; h<=endh; h++) {
        bool withinSegment = false;
        int startSegment = -1;
        SegmentList currentResults;
        for (int w=startw; w<endw; w++) {
			if (getat(h,w) > 0) {
                // Black pixel
                if (withinSegment)
                    continue;
                else {
                    startSegment = w;
                    withinSegment = true;
                    continue;
                }
            } else {
                // White pixel
                if (withinSegment) {
                    // Terminate current segment
                    currentResults.push_back(Segment(startSegment, w-1));
                    withinSegment = false;
                }
            }
        }
        if (currentResults.size() == nSegments) {
            if (nSegments == 0) {
                if (startMatchingRow == -1) {
                    // First matching line
                    startMatchingRow = endMatchingRow = h;
                } else {
                    // Augment area by pushing endMatchingRow
                    endMatchingRow = h;
                }
                continue; // No merghing of any segments list required
            } else {
                if (results.size() == 0) {
                    // First matching line!
                    results = currentResults;
                    startMatchingRow = endMatchingRow = h;
                } else {
                    // Already had one or more matching lines - try merging into existing results, unless doing so reduces the number of segments because it creates overlap
                    SegmentList slNew;
                    for (int i=0; i<nSegments; i++) {
                        Segment slCurrent = currentResults[i];
                        Segment slGlobal = results[i];
                        int existingStart = slGlobal.startPos;
                        int existingEnd = slGlobal.endPos;
                        int currentStart = slCurrent.startPos;
                        int currentEnd = slCurrent.endPos;
                        // Test overlap
                            // Current includes existing
                        if (((currentStart <= existingStart) && (currentEnd >= existingEnd))
                            // Current left side overlaps with existing
                            || ((currentStart >= existingStart-1) && (currentStart <= existingEnd+1))
                            // Current right side overlaps with existing
                            || ((currentEnd >= existingStart-1) && (currentEnd <= existingEnd+1))) {
                            // Merge segments!
                            int newStart = min(slGlobal.startPos, slCurrent.startPos);
                            int newEnd = max(slGlobal.endPos, slCurrent.endPos);
                            slNew.push_back(Segment(newStart, newEnd));
                        } else {
                            break;
                        }
                    } // iterate over current segments
                    if (slNew.size() == nSegments) {
                        // New combined segments list has the right count, but did segments get fused?
                        bool acceptNew = true;
                        for (int j=0; j<(int)slNew.size()-1; j++) {
                            Segment s = slNew[j], nextS = slNew[j+1];
                            if (s.endPos >= nextS.startPos - 1) {
                                acceptNew = false;
                                break; // Stop h loop
                            }
                        }
                        if (!acceptNew)
                            break;
                        results = slNew;
                        endMatchingRow = h;
                        continue;   // Keep growing the section
                    } // new combined has right number of segments
                } // already had segments
            } // nSegment > 0
        } // current results have right number of segments
        else {
            // Current horizontal slice didn't have the right number of elements: if we started compiling a section, end it and return results
            if (startMatchingRow >= 0) {
                break;
            }
        }
        // If we get here, we either still have no results at all, or current line merged successfully into what we had - keep going
    } // iterate over specified vertical range

    if (results.size() == nSegments) {
        // Build the box
        if (nSegments == 0) {
            box = CGRect(1, startMatchingRow, m_width, endMatchingRow - startMatchingRow + 1);
            segments = SegmentList();
        } else {
            box = CGRect(results[0].startPos,startMatchingRow, results[nSegments-1].endPos - results[0].startPos + 1, endMatchingRow - startMatchingRow + 1);
            segments = results;
        }
    } else {
        segments = SegmentList();
        box = CGRect(0,0,0,0);
	}
    
    // Before we return that box, check if we could get a taller box by iterating bottom-up
    // Go from starth towards endh until we find a line with the exact number of horizontal segments
    results = SegmentList();
    startMatchingRow = -1, endMatchingRow = -1;
    
    for (int h=endh; h>=starth; h--) {
        bool withinSegment = false;
        int startSegment = -1;
        SegmentList currentResults;
        for (int w=startw; w<endw; w++) {
			if (getat(h,w) > 0) {
                // Black pixel
                if (withinSegment)
                    continue;
                else {
                    startSegment = w;
                    withinSegment = true;
                    continue;
                }
            } else {
                // White pixel
                if (withinSegment) {
                    // Terminate current segment
                    currentResults.push_back(Segment(startSegment, w-1));
                    withinSegment = false;
                }
            }
        }
        if (currentResults.size() == nSegments) {
            if (nSegments == 0) {
                if (startMatchingRow == -1) {
                    // First matching line
                    startMatchingRow = endMatchingRow = h;
                } else {
                    // Augment area by pushing endMatchingRow
                    endMatchingRow = h;
                }
                continue; // No merghing of any segments list required
            } else {
                if (results.size() == 0) {
                    // First matching line!
                    results = currentResults;
                    startMatchingRow = endMatchingRow = h;
                } else {
                    // Already had one or more matching lines - try merging into existing results, unless doing so reduces the number of segments because it creates overlap
                    SegmentList slNew;
                    for (int i=0; i<nSegments; i++) {
                        Segment slCurrent = currentResults[i];
                        Segment slGlobal = results[i];
                        int existingStart = slGlobal.startPos;
                        int existingEnd = slGlobal.endPos;
                        int currentStart = slCurrent.startPos;
                        int currentEnd = slCurrent.endPos;
                        // Test overlap
                            // Current includes existing
                        if (((currentStart <= existingStart) && (currentEnd >= existingEnd))
                            // Current left side overlaps with existing
                            || ((currentStart >= existingStart-1) && (currentStart <= existingEnd+1))
                            // Current right side overlaps with existing
                            || ((currentEnd >= existingStart-1) && (currentEnd <= existingEnd+1))) {
                            // Merge segments!
                            int newStart = min(slGlobal.startPos, slCurrent.startPos);
                            int newEnd = max(slGlobal.endPos, slCurrent.endPos);
                            slNew.push_back(Segment(newStart, newEnd));
                        } else {
                            break;
                        }
                    } // iterate over current segments
                    if (slNew.size() == nSegments) {
                        // New combined segments list has the right count, but did segments get fused?
                        bool acceptNew = true;
                        for (int j=0; j<(int)slNew.size()-1; j++) {
                            Segment s = slNew[j], nextS = slNew[j+1];
                            if (s.endPos >= nextS.startPos - 1) {
                                acceptNew = false;
                                break; // Stop h loop
                            }
                        }
                        if (!acceptNew)
                            break;
                        results = slNew;
                        endMatchingRow = h;
                        continue;   // Keep growing the section
                    } // new combined has right number of segments
                } // already had segments
            } // nSegment > 0
        } // current results have right number of segments
        else {
            // Current horizontal slice didn't have the right number of elements: if we started compiling a section, end it and return results
            if (startMatchingRow >= 0) {
                break;
            }
        }
        // If we get here, we either still have no results at all, or current line merged successfully into what we had - keep going
    } // iterate over specified vertical range

    // Did we improve over finding from above?
    if ((results.size() == nSegments) && (startMatchingRow - endMatchingRow + 1 > box.size.height)) {
        // Build the box
        if (nSegments == 0) {
            box = CGRect(1, endMatchingRow, m_width, startMatchingRow - endMatchingRow + 1);
            segments = SegmentList();
        } else {
            box = CGRect(results[0].startPos,endMatchingRow, results[nSegments-1].endPos - results[0].startPos + 1, startMatchingRow - endMatchingRow + 1);
            segments = results;
        }
    }
    
    return box;
}

ocrparser::CGRect cimage::linetest_BoxForNHorizontalSegments(int nSegments, float linecenter, float linewidth, SegmentList &segments) {
    // m_height is actual height + 2 (because of dummy white margin of one pixel above and below)
    // 2016-04-22 switching from floor to round: doesn't make sense to use 0 when math yields 0.9, does it?
	int starth = 1 + round((m_height - 2)*(linecenter-(linewidth/2))/100);
    // 2016-04-22 line below was commented out, no idea why. Seems sensible not to fail the call in overflow cases.
    if (starth <= 0) starth = 1;
    if (starth >= m_height) starth = m_height - 1;
    // 2016-04-22 switching from ceil to round: doesn't make sense to use 1 when math yields 0.2, does it?
	int endh   =  1 + round((m_height - 2)*(linecenter+(linewidth/2))/100);
    if (endh > m_height - 1) endh = m_height - 1;
    // Make sure we test a line of at least one pixel
    if (endh <= starth) {
        // If we are closer to bottom, get closer by subs 1 to bottom range, otherwise start 1 lower
        if ((starth - 1) < ((m_height - 1)/2)) {
            endh = starth + 1;
        } else {
            starth = endh - 1;
        }
        // Re-check for validity, just in case
        if (starth >= m_height)
            starth = m_height - 1;
        if (endh >= m_height)
            endh = m_height - 1;
        if (starth < 1)
            starth = 1;
        if (endh < 1)
            endh = 1;
    }

    
    return linetest_BoxForNHorizontalSegments_absolute(nSegments, starth, endh, segments);
}


SegmentList cimage::linetest_horizontal_absolute(int starth, int endh) {

    SegmentList results;
    
    if ((starth >= m_height) || (endh >= m_height) || (starth <= 0) || (endh <= 0))
        return results;
        
    int startw = 0;
	int endw   = m_width;
    
	int part_start = -1;
	bool last_state = false;

	for(int w=startw; w<endw; w++) {
		int count = 0;
		for(int h=starth; h<endh; h++) {
			if (getat(h,w) > 0) {
				count ++;
                break;
			}
            if (count > 0)
                break;
		}

		if (count > 0) {
            if (!last_state) {
                // start of part
                part_start = w;
                last_state = true;
            }
		} else {
            if (last_state) {
                // end of part
                results.push_back(Segment(part_start, w-1));
                last_state = false;
            }
        }
	}

    // segment till the end
    if (last_state) {
        results.push_back(Segment(part_start, endw-1));
    }
    
	return results;
}

cimage* cimage::inverse() {
    // PatrickQ 04-25-2013: for the inverse need to invert neighbors4. For example if the class has neighbor4=false, in the invert you want neigbors4=true so that the white doesn't "leak out" through a diagonal barrier
    cimage* out = new cimage(m_height, m_width, !m_neighbors4);
	for(int i=0; i<m_height*m_width; i++) {
		out->m_buffer[i] = 1 - m_buffer[i];
	}

	return out;
}

//returns: the percentage of the bottom right pixel
float cimage::bottom_right_corner_test(CCTYPE concomp) {
        /*min_h, min_w, max_h, max_w, size*/
	int max_h = concomp[2];
	int min_h = concomp[0];
	int max_w = concomp[3];
	int min_w = concomp[1];
    for(int w=max_w; w>=min_w; w--) {
        for(int h=max_h; h>=min_h; h--) {
            if (getat(h,w) > 0) {
                float percentage = (float(max_h)-float(h))/(float(max_h)-float(min_h));
#if DEBUG_SINGLELETTER
                cout << "PERCENTAGE" << percentage << endl;
#endif
                if ((getat(h-1,w) > 0) || (percentage < 0.1)) {
                    return percentage;
                }
            }
        }
    }

	//should not get here
	return -1;
}

float cimage::top_right_corner_test(CCTYPE concomp) {
        /*min_h, min_w, max_h, max_w, size*/
	int max_h = concomp[2];
	int min_h = concomp[0];
	int max_w = concomp[3];
	for(int h=min_h; h<=max_h; h++) {
		if (getat(h,max_w) > 0) {
			return (float(h)-float(min_h))/(float(max_h)-float(min_h));
		}
	}

	//should not get here
	return -1;
}

float cimage::flat_right_test(CCTYPE concomp) {
        /*min_h, min_w, max_h, max_w, size*/
	int max_h = concomp[2];
	int min_h = concomp[0];
	int max_w = concomp[3];
	int min_w = concomp[1];

    int right_w = ceil(max_w - 0.1*(max_w - min_w));
    int count = 0;
	for(int h=min_h; h<=max_h; h++) {
		if (getat(h,right_w) > 0) {
            ++count;
		}
	}

    return float(count)/(float(max_h)-float(min_h)+1);
}

void cimage::integrate(CCTYPE concomp) {
    if (m_integrated) {
        //already calculated
        return;
    }

	int max_h = concomp[2];
	int min_h = concomp[0];
	int max_w = concomp[3];
	int min_w = concomp[1];
    cimage* img;
    if (m_cached_big_component) {
        img = m_cached_big_component;
    } else {
        img = this;
    }

   	m_integrated = new vector<int>(max_h-min_h+1, 0);

	int row=0;
	for(int h=min_h; h<=max_h; h++) {
		for(int w=min_w; w<=max_w; w++) {
			if (img->getat(h,w) > 0) {
				(*m_integrated)[row] += 1;
			}
		}
		row++;
	}
}

HORZTESTMAX_TYPE cimage::horz_integration_maxtest(CCTYPE concomp, float max_threshold) {
    integrate(concomp);

    //find the median
    vector<int> temp(*m_integrated);
	sort(temp.begin(), temp.end());
    /* //MAJORITY - was not better than median
    int majority_count = 0; int majority_value = 0;
    int current_count  = 0; int current_value  = 0;
    for (vector<int>::iterator iter = temp.begin(); iter != temp.end(); ++iter) {
        if ((*iter) == current_value) {
            ++current_count;
        } else {
            if (current_count > majority_count) {
                majority_count = current_count;
                majority_value = current_value;
            }

            current_value = (*iter);
            current_count = 1;
        }
    }
    cout << "MAJORITY: " << majority_value << " (count: " << majority_count << ")" << endl;
    int median_value = majority_value;
    */
    int median_value = temp[int(float(temp.size())/3)]; //took third and not half - seems to be more robust as we use this test for "t" and "f"
#if DEBUG_SINGLELETTER
    cout << "MEDIAN: " << median_value << endl;
#endif

    bool is_up = true;
    int last_val = 0, cur_val;
    int num_identical = 0;
    HORZTESTMAX_TYPE retval;
    size_t i;
    for(i=0; i<m_integrated->size(); i++) {
        cur_val = (*m_integrated)[i];
        if (i==0) {
            last_val = cur_val;
            continue;
        }
        if (is_up && (cur_val < last_val)) {
            is_up = false;
            if ((float(last_val) > max_threshold*float(median_value)) && (last_val-median_value>1)) {
                //TODO: add max value, use also num_identical
	            HORZTEST_TYPE size_height_width(3, 0.0);
                size_height_width[0] = float(1+num_identical)/float(m_integrated->size());
                size_height_width[1] = (float(i-1 + i-1-num_identical)/float(2)+0.5)/float(m_integrated->size());
                size_height_width[2] = float(last_val)/float(median_value);
                retval.push_back(size_height_width);
            }
        } else if (cur_val > last_val) {
            is_up = true;
        }

        if (cur_val == last_val) {
            num_identical++;
        } else {
            num_identical = 0;
        }

        last_val = cur_val;
    }

    if ((is_up) && (float(last_val) > max_threshold*float(median_value)) && (last_val-median_value>1)) {
        //add the last point as a max
        HORZTEST_TYPE size_height_width(3, 0.0);
        size_height_width[0] = float(1+num_identical)/float(m_integrated->size());
        size_height_width[1] = (float(i-1 + i-1-num_identical)/float(2)+0.5)/float(m_integrated->size());
        size_height_width[2] = float(last_val)/float(median_value);
        retval.push_back(size_height_width);
    }

    return retval;
}

HORZTEST_TYPE cimage::horz_integration_test(CCTYPE concomp) {
    integrate(concomp);

	int maxval = *max_element(m_integrated->begin(), m_integrated->end());
	int minval = 0; //the following code makes sure that the minimal value is not in the start or the end
	vector<int>::iterator minbegin = m_integrated->begin(), minend = m_integrated->end();

	/*for (unsigned int i=0; i<m_integrated->size(); i++) {
		cout << (*m_integrated)[i] << ",";
	}*/
	while (minbegin != minend) {
		minval = *min_element(minbegin, minend);
		if (*(minbegin) == minval) {
			minbegin ++;
			continue;
		}
		if (*(minend-1) == minval) {
			minend --;
			continue;
		}
		break;
	}

	vector<int> maxcoords;
	vector<int> mincoords;
	bool min_state = false, max_state = false;
	bool max_contiguous = true, min_contiguous = true;
    
#if DEBUG_SINGLELETTER
    cout << "start " << distance(m_integrated->begin(), minbegin) << ", end:"<<distance(m_integrated->begin(), minend) << endl;
#endif
	for(int i=(int)distance(m_integrated->begin(), minbegin); i<(int)distance(m_integrated->begin(), minend); i++) {
		if ((*m_integrated)[i] == minval) {
			if ((!min_state) && (mincoords.size() > 0)) {
				min_contiguous = false;
			}
			mincoords.push_back(i);
			min_state = true;
		} else {
			min_state = false;
		}
	}

	for(size_t i=0; i<m_integrated->size(); i++) {
		if ((*m_integrated)[i] == maxval) {
			if ((!max_state) && (maxcoords.size() > 0)) {
				max_contiguous = false;
			} else { //added the else so that the statistics will be generated only on the first max place (relevant for t and f)
    			maxcoords.push_back((int)i);
    			max_state = true;
            }
		} else {
			max_state = false;
		}
    }

	HORZTEST_TYPE retval(8, 0.0);
    //is minimum contiguous
	retval[0] = (min_contiguous && (mincoords.size()>0)) ? 1 : 0;
    //is maximum contiguous
	retval[1] = (max_contiguous && (maxcoords.size()>0)) ? 1 : 0;
    // how long is the minimum in relation to the whole letter?
	retval[2] = float(mincoords.size())/float(m_integrated->size());
    // how long is the maximum in relation to the whole letter?
	retval[3] = float(maxcoords.size())/float(m_integrated->size());
    //where is the minimum located (height in %)
	retval[4] = (0.5+(float(accumulate(mincoords.begin(), mincoords.end(), 0))/float(mincoords.size())))/float(m_integrated->size());
    //where is the maximum located (height in %)
	retval[5] = (0.5+(float(accumulate(maxcoords.begin(), maxcoords.end(), 0))/float(maxcoords.size())))/float(m_integrated->size());
    //how thick is the minimum is relation to the average thickness
    if (mincoords.size() == 0) {
        retval[6] = retval[4]; //nan
    } else {
    	retval[6] = float(minval)/((float(accumulate(m_integrated->begin(), m_integrated->end(), 0))/float(m_integrated->size())));
    }
    //how thick is the maximum is relation to the average thickness
    if (maxcoords.size() == 0) {
        retval[7] = retval[5]; //nan
    } else {
    	retval[7] = float(maxval)/((float(accumulate(m_integrated->begin(), m_integrated->end(), 0))/float(m_integrated->size())));
    }

	return retval;
}

//pq12
bool side_test_topbottom_internal(cimage* img, CCTYPE& concomp, int direction, float requiredLen) {
    
    int height = concomp[2] - concomp[0] + 1;
    int width = concomp[3] - concomp[1] + 1;
    
    int starth = concomp[0]; // ymin
    int endh =   starth + round(height * 0.10); // ymax
    int startw = concomp[1]; // xmin
    int endw =   concomp[3]; // xmax
    
	int maxCount = 0;
    // Require bar at least requiredLen % of total width (typical value for requiredLen is 0.90 for 90%)
    int countRequired = floor(width * requiredLen);
	
    // First step: find the left side of the top line, anywhere within the top 10% of the rect
    for (int x=startw; x<endw; x++) {
        for (int y=starth; y<endh; y++) {
            int count = 0;
            
            // Test what we can do with this as start point for the hor bar
            if (img->getat(y,x) > 0) {
                // Now see how wide a line we can get starting here
                int lx=x+1; int ly=y;
                count = 1;
                while (lx < endw) {
                    if (img->getat(ly,lx) > 0) {
                        count++;
                        lx++; continue;
                    } else if ((ly>0) && (img->getat(ly-1,lx) > 0)) {
                        count++;
                        ly--;
                        lx++; continue;
                    } else if ((ly<(endh-1)) && (img->getat(ly+1,lx) > 0)) {
                        count++;
                        ly++;
                        lx++; continue;
                    }
                    // Failed to find another dot - abort
                    break;
                }
            }
            
            // See what we got
            if (count > maxCount) {
                if (count >= countRequired)
                    return true;
                else 
                    maxCount = count;
            }
            
            if (count > 0) {
                // We found a pixel at current y, no need to continue down the y axis
                continue;
            }
        }
    }
    return false;
} // side_test_topbottom_internal

// Assumes that we are testing the right side only for now, once it works I'll make it work both sides
bool side_test_leftright_internal(cimage* img, CCTYPE& concomp, int direction, float requiredLen) {
    
    int height = concomp[2] - concomp[0] + 1;
    int width = concomp[3] - concomp[1] + 1;
    
    int starth = concomp[0]; // ymin
    int endh =   concomp[2]; // ymax
    int startw = ((direction == -1)? concomp[3] - round(width * 0.10) : concomp[1]); // xmin
    int endw =   ((direction == -1)? concomp[3]                       : concomp[1] + round(width * 0.10)); // xmax
    
	int maxCount = 0;
    // Require bar at least requiredLen % of total height (typical value for requiredLen is 0.90 for 90%)
    int countRequired = floor(height * requiredLen);
	
    // Scan all rows top down to look for rows with some black pixels within the specified X range (e.g. rightmost 10% of the rect)
    //pq11 doesn't make sense to iterate through entire range, required length should dictate stopping earlier
    for (int y=starth; y<endh; y++) {
        // At each y position find a black pixel along X axis
        for (int x=startw; x<endw; x++) {
            int count = 0;
            
            // Test what we can do with this as start point for the hor bar
            if (img->getat(y,x) > 0) {
                // Found a black pixels! Now see how deep a line we can get starting here
                int ly=y+1; int lx=x;
                count = 1;
                while (ly < endh) {
                    if (img->getat(ly,lx) > 0) {
                        count++;
                        ly++; continue;
                    } else if ((lx>startw) && (img->getat(ly,lx-1) > 0)) {
                        count++;
                        lx--;
                        ly++;
                        continue;
                    } else if ((lx<(endw-1)) && (img->getat(ly,lx+1) > 0)) {
                        count++;
                        lx++;
                        ly++;
                        continue;
                    }
                    // Failed to find another dot - abort
                    break;
                }
            }
            
            // See what we got
            if (count > maxCount) {
                if (count >= countRequired)
                    return true;
                else 
                    maxCount = count;
            }
            
            if (count > 0) {
                // We found a pixel at current x, no need to continue down the x axis
                continue;
            }
        }
    }
    return false;
} // side_test_leftright_internal


//pq103

void openings_test_topbottom_internal(cimage* img, CCTYPE& concomp, float start_position, float end_position, cimage::BoundsRequired start_bound_required, cimage::BoundsRequired end_bound_required, bool bounds_in_range, float test_depth, int direction, float& out_max_depth, float& out_width, float& out_max_width, float& out_area, float& out_max_depth_coord, float& out_min_width, float& out_minwidth_start, float& out_minwidth_end, float& out_minwidth_depth) {
    int max_h = concomp[2];
    int min_h = concomp[0];
    int max_w = concomp[3];
    int min_w = concomp[1];
    
    vector<int> widths;

    //find the max depth
	int h, w;
    //int halfsize = (max_w-min_w)/4;
    int start_idx = min_w + (start_position*(max_w-min_w+1));
    int end_idx = min_w + (end_position*(max_w-min_w+1));
    int end_depth_idx = (direction == 1) ? min_h+(test_depth*(max_h-min_h+1)) : max_h-(test_depth*(max_h-min_h+1));
    // If bounds_in_range is set we look for bounds only within the same width subrange as the opening
    int min_w_for_bounds = min_w;
    int max_w_for_bounds = max_w;
    if (bounds_in_range) {
        min_w_for_bounds = start_idx;
        max_w_for_bounds = end_idx;
    }
    int max_depth = -1; // This is the top of the chosen opening, measured in pixels from the edge
    int max_bounded_depth = -1; // This is the actual bounded depth, which we try to maximize before chosing which opening to pick
    int max_depth_idx = -1;
    int start_h;
    int end_h;
    if (bounds_in_range) {
        start_h = (direction == 1) ? start_idx : end_idx;
        end_h   = (direction == 1) ? end_idx+1 : start_idx-1;
    } else {
        start_h = (direction == 1) ? min_h : max_h;
        end_h   = (direction == 1) ? max_h+1 : min_h-1;
    }

    for(w=start_idx; w<=end_idx; w++) {
        for(h =  start_h ; h != end_h ; h += direction) {
            //only if found a bound it is counted a a depth:
			if ((img->getat(h,w) > 0) || (h == end_depth_idx)) { 
                //go back one step
                h -= direction;
                int current_top = (direction == 1)? h-min_h+1 : max_h-h+1;
                // Measure the depth of this opening per the requirements on both sides
                int depth = 0;
                
                // Keep stepping one pixel back as long as bound requirements match on both sides
                while (((direction == -1)? (h <= start_h):(h >= start_h))) {
                    float start_bound = false, end_bound = false;
                    int w2;
                    for (w2=w; w2>=min_w_for_bounds; w2--) {
                        if (img->getat(h,w2) > 0) {
                            start_bound = true;
                            break;
                        }
                    }
                    for (w2=w+1; w2<=max_w_for_bounds; w2++) {
                        if (img->getat(h,w2) > 0) {
                            end_bound = true;
                            break;
                        }
                    }

                    if(((start_bound_required == cimage::Unbound && !start_bound) || (start_bound_required == cimage::Bound && start_bound) || (start_bound_required == cimage::Any)) && 
                        ((end_bound_required   == cimage::Unbound && !end_bound)   || (end_bound_required   == cimage::Bound && end_bound)   || (end_bound_required == cimage::Any)))  
                    {
                        //take this depth into account only if the bounds are right
                        depth++;
                        h -= direction;
                    } else {
                        break;
                    }
                } // stepping back one pixel at a time
                if ((depth > 0) && (depth > max_bounded_depth)) {
                    max_bounded_depth = depth;
                    max_depth = current_top;
                    max_depth_idx = w;
                }

				break; //no need to go deeper - as we already found the boundary
			}
		}
	}

    if (max_depth == -1) {
        out_max_depth = out_width = out_max_width = out_area = out_max_depth_coord = out_min_width = 
                        out_minwidth_start = out_minwidth_end = out_minwidth_depth = 0;
        return;
    }

    int width;   // calculates the width in each depth
    int minwidth = 0, minwidth_start_idx = 0, minwidth_end_idx = 0, width_start_idx = 0, width_end_idx = 0, minwidth_depth = 0;
    int end_h2 = (direction == 1)? min_h+max_depth : max_h-max_depth;
    for(h = start_h; h != end_h2; h += direction) {
        width = 0;
        float start_bound = false, end_bound = false;
        for (w=max_depth_idx; w>=min_w; w--) {
    		if (img->getat(h,w) > 0) {
                start_bound = true;
                w++;
                break;
            }
            width++;
        }
        width_start_idx = w;
        for (w=max_depth_idx+1; w<=max_w; w++) {
    		if (img->getat(h,w) > 0) {
                end_bound = true;
                w--;
                break;
            }
            width++;
        }
        width_end_idx = w;
        
        //if ((!start_bound_required || start_bound) && (!end_bound_required || end_bound)) {
        if(((start_bound_required == cimage::Unbound && !start_bound) || (start_bound_required == cimage::Bound && start_bound) || (start_bound_required == cimage::Any)) && 
           ((end_bound_required   == cimage::Unbound && !end_bound)   || (end_bound_required   == cimage::Bound && end_bound)   || (end_bound_required == cimage::Any)))  {
            widths.push_back(width);

            if ((minwidth == 0) || (width < minwidth)) {
                minwidth = width;
                minwidth_start_idx = width_start_idx;
                minwidth_end_idx = width_end_idx;
                minwidth_depth = (direction == 1)? h-min_h+1 : max_h-h+1;
            }

        } else {
            // Bounds condition is NOT met
            // Max depth can be AT MOST from the next
            // DEBUG cout << "METMET: from: " << max_depth << " to: " << abs(h-end_h2)-1 << " at: " << max_depth_idx << endl;
            max_depth = abs(h-end_h2)-1;
        }
    }

    out_max_depth = max_depth;
    out_max_depth_coord = max_depth_idx-min_w;/*((float)(max_depth_idx-min_w))/((float)(max_w-min_w+1));*/
    if ((out_max_depth < 1) || (widths.size() < 1)) {
        out_width = out_max_width = out_area = out_min_width = out_minwidth_start = out_minwidth_end = out_minwidth_depth = 0;
    } else {
        out_min_width = minwidth;
        out_minwidth_depth = minwidth_depth;
        out_minwidth_start = minwidth_start_idx-min_w;/*((float)minwidth_start_idx-min_w)/((float)(max_w-min_w+1));*/
        out_minwidth_end   = minwidth_end_idx-min_w;/*((float)minwidth_end_idx-min_w)/  ((float)(max_w-min_w+1));*/
        out_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        out_max_width = *max_element(widths.begin(), widths.end());
        out_area = accumulate(widths.begin(), widths.end(), 0);
    }
}

//left is direction=1
void openings_test_leftright_internal(cimage* img, CCTYPE& concomp, float start_position, float end_position, cimage::BoundsRequired start_bound_required, cimage::BoundsRequired end_bound_required, bool bounds_in_range, float test_depth, int direction, float& out_max_depth, float& out_width, float& out_max_width, float& out_area, float& out_max_depth_coord, float& out_min_width, float& out_minwidth_start, float& out_minwidth_end, float& out_minwidth_depth) {
    int max_h = concomp[2];
    int min_h = concomp[0];
    int max_w = concomp[3];
    int min_w = concomp[1];
    
    vector<int> widths;

    //find the max depth
	int h, w;
    //int halfsize = (max_h-min_h)/4;
    int start_idx = min_h + (start_position*(max_h-min_h+1));
    int end_idx = min_h + (end_position*(max_h-min_h+1));
    // If bounds_in_range is set we look for bounds only within the same height subrange as the opening
    int min_h_for_bounds = min_h;
    int max_h_for_bounds = max_h;
    if (bounds_in_range) {
        min_h_for_bounds = start_idx;
        max_h_for_bounds = end_idx;
    }
    int end_depth_idx = (direction == 1) ? min_w+(test_depth*(max_w-min_w+1)) : max_w-(test_depth*(max_w-min_w+1));
    int max_depth = -1;
    int max_depth_idx = -1;
    int start_w = (direction == 1) ? min_w : max_w;
    int end_w   = (direction == 1) ? max_w+1 : min_w-1;

    for(h=start_idx; h<=end_idx; h++) {
        for(w =  start_w ; w != end_w ; w += direction) {
            //only if found a bound it is counted a a depth
			if ((img->getat(h,w) > 0) || (w == end_depth_idx)) { 
                //go back one step
                w -= direction;
                int depth = (direction == 1)? w-min_w+1 : max_w-w+1;
		        if (depth > max_depth) {
                    float start_bound = false, end_bound = false;
                    int h2;
                    for (h2=h; h2>=min_h_for_bounds; h2--) {
                        if (img->getat(h2,w) > 0) {
                            start_bound = true;
                            break;
                        }
                    }
                    for (h2=h+1; h2<=max_h_for_bounds; h2++) {
                        if (img->getat(h2,w) > 0) {
                            end_bound = true;
                            break;
                        }
                    }
                    if(((start_bound_required == cimage::Unbound && !start_bound) || (start_bound_required == cimage::Bound && start_bound) || (start_bound_required == cimage::Any)) && 
                       ((end_bound_required   == cimage::Unbound && !end_bound)   || (end_bound_required   == cimage::Bound && end_bound)   || (end_bound_required == cimage::Any)))  {
                        max_depth = depth;
                        max_depth_idx = h;
                    }
                }

				break; //no need to go deeper - as we already found the boundary
			}
		}
	}

    if (max_depth == -1) {
        out_max_depth = out_width = out_max_width = out_area = out_max_depth_coord = out_min_width = 
                        out_minwidth_start = out_minwidth_end = out_minwidth_depth = 0;
        return;
    }

    int width;   // calculates the width in each depth
    int minwidth = 0, minwidth_start_idx = 0, minwidth_end_idx = 0, width_start_idx = 0, width_end_idx = 0, minwidth_depth = 0;
    int end_w2 = (direction == 1)? min_w+max_depth : max_w-max_depth;
    for(w = start_w; w != end_w2; w += direction) {
        width = 0;
        float start_bound = false, end_bound = false;
        for (h=max_depth_idx; h>=min_h; h--) {
    		if (img->getat(h,w) > 0) {
                start_bound = true;
                h++;
                break;
            }
            width++;
        }
        width_start_idx = h;
        for (h=max_depth_idx+1; h<=max_h; h++) {
    		if (img->getat(h,w) > 0) {
                end_bound = true;
                h--;
                break;
            }
            width++;
        }
        width_end_idx = h;
        
        if(((start_bound_required == cimage::Unbound && !start_bound) || (start_bound_required == cimage::Bound && start_bound) || (start_bound_required == cimage::Any)) && 
           ((end_bound_required   == cimage::Unbound && !end_bound)   || (end_bound_required   == cimage::Bound && end_bound)   || (end_bound_required == cimage::Any)))  {
            // DEBUG cout << "METMET: " << w << " OK" << endl;
            widths.push_back(width);

            if ((minwidth == 0) || (width < minwidth)) {
                minwidth = width;
                minwidth_start_idx = width_start_idx;
                minwidth_end_idx = width_end_idx;
                minwidth_depth = (direction == 1)? w-min_w+1 : max_w-w+1;
            }

        } else {
            // Bounds condition is NOT met
            // Max depth can be AT MOST from the next
            // DEBUG cout << "METMET: (" << w << ") from: " << max_depth << " to: " << abs(w-end_w2)-1 << " at: " << max_depth_idx << endl;
            max_depth = abs(w-end_w2)-1;
        }
    }

    out_max_depth = max_depth;
    out_max_depth_coord = max_depth_idx-min_h;/*((float)(max_depth_idx-min_h))/((float)(max_h-min_h+1));*/
    if ((out_max_depth < 1) || (widths.size() < 1)) {
        out_width = out_max_width = out_area = out_min_width = out_minwidth_start = out_minwidth_end = out_minwidth_depth = 0;
    } else {
        out_min_width = minwidth;
        out_minwidth_depth = minwidth_depth;
        out_minwidth_start = minwidth_start_idx-min_h;//((float)minwidth_start_idx-min_h)/((float)(max_h-min_h+1));
        out_minwidth_end   = minwidth_end_idx-min_h;//((float)minwidth_end_idx-min_h)/  ((float)(max_h-min_h+1));
        out_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        out_max_width = *max_element(widths.begin(), widths.end());
        out_area = accumulate(widths.begin(), widths.end(), 0);
    }
}

bool cimage::side_test(bool only_main_component, CCTYPE concomp, bool is_topbottom, int direction, float requiredLen) {
    cimage* img;
    if (only_main_component && m_cached_big_component) {
        img = m_cached_big_component;
    } else {
        img = this;
    }

    if (is_topbottom) {
        return side_test_topbottom_internal(img, concomp, direction, requiredLen);
    } else {
        return side_test_leftright_internal(img, concomp, direction, requiredLen);
    }
    return false;
}

void cimage::openings_test(bool only_main_component, CCTYPE concomp, float start_position, float end_position, BoundsRequired start_bound_required, BoundsRequired end_bound_required, bool bounds_in_range, float test_depth, bool is_topbottom, int direction, float& max_depth, float& width, float& max_width, float& area, float& max_depth_coord, float& min_width, float& minwidth_start, float& minwidth_end, float& minwidth_depth) {
    cimage* img;
    if (only_main_component && m_cached_big_component) {
        img = m_cached_big_component;
    } else {
        img = this;
    }

    if (is_topbottom) {
        openings_test_topbottom_internal(img, concomp, start_position, end_position, start_bound_required, end_bound_required, bounds_in_range, test_depth, direction, max_depth, width, max_width, area, max_depth_coord, min_width, minwidth_start, minwidth_end, minwidth_depth);
    } else {
        openings_test_leftright_internal(img, concomp, start_position, end_position, start_bound_required, end_bound_required, bounds_in_range, test_depth, direction, max_depth, width, max_width, area, max_depth_coord, min_width, minwidth_start, minwidth_end, minwidth_depth);
    }
}

void old_openings_test_topbottom_internal(cimage* img, CCTYPE& concomp, float start_position, float end_position, bool bounds_required, float letter_thickness, int direction, float& out_max_depth, float& out_width, float& out_opening_width, float& out_max_width, float& out_area) {
    int max_h = concomp[2];
    int min_h = concomp[0];
    int max_w = concomp[3];
    int min_w = concomp[1];

    vector<int> widths;

    //find the max depth
	int h, w;
    //int halfsize = (max_w-min_w)/4;
    int start_idx = min_w + (start_position*(max_w-min_w+1));
    int end_idx = min_w + (end_position*(max_w-min_w+1));
    int max_depth = -1;
    int max_depth_idx = -1;
    int start_h = (direction == 1) ? min_h : max_h;
    int end_h   = (direction == 1) ? max_h+1 : min_h-1;
    int bounds; // counts how many bounds are detected for each width

    for(w=start_idx; w<=end_idx; w++) {
        for(h =  start_h ; h != end_h ; h += direction) {
			if (img->getat(h,w) > 0) { //only if found a bound it is counted a a depth
                //go back one step
                h -= direction;
                int depth = (direction == 1)? h-min_h+1 : max_h-h+1;
		        if (depth > max_depth) {
                    if (bounds_required) {
                        bounds = 0;
                        int w2;
                        for (w2=w; w2>=min_w; w2--) {
                            //cout << "{" << h << ":" << w2 << "}";
                            if (img->getat(h,w2) > 0) {
                                bounds++;
                                break;
                            }
                        }
                        for (w2=w+1; w2<=max_w; w2++) {
                            //cout << "{" << h << ":" << w2 << "}";
                            if (img->getat(h,w2) > 0) {
                                bounds++;
                                break;
                            }
                        }
                        if (bounds == 2) {
                            max_depth = depth;
                            max_depth_idx = w;
                        }
                    } else { //no bounds required
                        max_depth = depth;
                        max_depth_idx = w;
                    }
                }

				break; //no need to go deeper - as we already found the boundary
			}
		}
	}

    if (max_depth == -1) {
        out_max_depth = out_opening_width = out_width = out_max_width = out_area = 0;
        return;
    }

#if DEBUG_SINGLELETTER
    cout << "MAX DEPTH (topbottom): "<< max_depth << " (@"<<max_depth_idx << ")\n";
#endif

    float opening_width = 0;
    int width;   // calculates the width in each depth
    int end_h2 = (direction == 1)? min_h+max_depth : max_h-max_depth;
    for(h = start_h; h != end_h2; h += direction) {
        width = 0;
        bounds = 0;
        for (w=max_depth_idx; w>=min_w; w--) {
    		if (img->getat(h,w) > 0) {
                bounds++;
                break;
            }
            width++;
        }
        for (w=max_depth_idx+1; w<=max_w; w++) {
    		if (img->getat(h,w) > 0) {
                bounds++;
                break;
            }
            width++;
        }
        if ((! bounds_required) || (bounds == 2)) {
            widths.push_back(width);
        }
        if ((opening_width == 0) && (float(widths.size())/float(max_h-min_h+1) >= letter_thickness)) {
            opening_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        }
    }

    out_max_depth = max_depth;
    if ((out_max_depth < 1) || (widths.size() < 1)) {
        out_opening_width = out_width = out_max_width = out_area = 0;
    } else {
        out_opening_width = opening_width;
        out_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        out_max_width = *max_element(widths.begin(), widths.end());
        out_area = accumulate(widths.begin(), widths.end(), 0);
    }
}

//left is direction=1
void old_openings_test_leftright_internal(cimage* img, CCTYPE& concomp, float start_position, float end_position, bool bounds_required, float letter_thickness, int direction, float& out_max_depth, float& out_width, float& out_opening_width, float& out_max_width, float& out_area) {
    int max_h = concomp[2];
    int min_h = concomp[0];
    int max_w = concomp[3];
    int min_w = concomp[1];

    vector<int> widths;

    //find the max depth
	int h, w;
    //int halfsize = (max_h-min_h)/4;
    int start_idx = min_h + (start_position*(max_h-min_h+1));
    int end_idx = min_h + (end_position*(max_h-min_h+1));
    int max_depth = -1;
    int max_depth_idx = -1;
    int start_w = (direction == 1) ? min_w : max_w;
    int end_w   = (direction == 1) ? max_w+1 : min_w-1;
    int bounds; // counts how many bounds are detected for each width

    for(h=start_idx; h<=end_idx; h++) {
        for(w =  start_w ; w != end_w ; w += direction) {
			if (img->getat(h,w) > 0) { //only if found a bound it is counted a a depth
                //go back one step
                w -= direction;
                int depth = (direction == 1)? w-min_w+1 : max_w-w+1;
		        if (depth > max_depth) {
                    if (bounds_required) {
                        bounds = 0;
                        int h2;
                        for (h2=h; h2>=min_h; h2--) {
                            if (img->getat(h2,w) > 0) {
                                bounds++;
                                break;
                            }
                        }
                        for (h2=h+1; h2<=max_h; h2++) {
                            if (img->getat(h2,w) > 0) {
                                bounds++;
                                break;
                            }
                        }
                        if (bounds == 2) {
                            max_depth = depth;
                            max_depth_idx = h;
                        }
                    } else { //no bounds required
                        max_depth = depth;
                        max_depth_idx = h;
                    }
                }

				break; //no need to go deeper - as we already found the boundary
			}
		}
	}

    if (max_depth == -1) {
        out_max_depth = out_opening_width = out_width = out_max_width = out_area = 0;
        return;
    }

#if DEBUG_SINGLELETTER
    cout << "MAX DEPTH (leftright): "<< max_depth << " (@"<<max_depth_idx << ")\n";
#endif

    float opening_width = 0;
    int width;   // calculates the width in each depth
    int end_w2 = (direction == 1)? min_w+max_depth : max_w-max_depth;
    for(w = start_w; w != end_w2; w += direction) {
        width = 0;
        bounds = 0;
        for (h=max_depth_idx; h>=min_h; h--) {
    		if (img->getat(h,w) > 0) {
                bounds++;
                break;
            }
            width++;
        }
        for (h=max_depth_idx+1; h<=max_h; h++) {
    		if (img->getat(h,w) > 0) {
                bounds++;
                break;
            }
            width++;
        }
        if ((! bounds_required) || (bounds == 2)) {
            widths.push_back(width);
        }

        if ((opening_width == 0) && (float(widths.size())/float(max_w-min_w+1) >= letter_thickness)) {
            opening_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        }
    }

    out_max_depth = max_depth;
    if ((out_max_depth < 1) || (widths.size() < 1)) {
        out_opening_width = out_width = out_max_width = out_area = 0;
    } else {
        out_opening_width = opening_width;
        out_width = (float(accumulate(widths.begin(), widths.end(), 0))/float(widths.size()));
        out_max_width = *max_element(widths.begin(), widths.end());
        out_area = accumulate(widths.begin(), widths.end(), 0);
    }
}

void cimage::old_openings_test(CCTYPE concomp, float start_position, float end_position, bool bounds_required, float letter_thickness, bool is_topbottom, int direction, float& max_depth, float& width, float& opening_width, float& max_width, float& area) {
    cimage* img;
    if (m_cached_big_component) {
        img = m_cached_big_component;
    } else {
        img = this;
    }

    if (is_topbottom) {
        old_openings_test_topbottom_internal(img, concomp, start_position, end_position, bounds_required, letter_thickness, direction, max_depth, width, opening_width, max_width, area);
    } else {
        old_openings_test_leftright_internal(img, concomp, start_position, end_position, bounds_required, letter_thickness, direction, max_depth, width, opening_width, max_width, area);
    }
}


