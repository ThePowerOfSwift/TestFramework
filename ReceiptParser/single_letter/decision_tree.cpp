#define CODE_CPP 1

//from numpy import array, ones
#include <vector>
#include <limits>
#include <map>
#include <iostream>
#include <stdlib.h>

#define CODE_CPP 1

#include "cimage.h"
#include "slcommon.h"

enum {
    HORZMAX_SIZE_COORD = 0,
    HORZMAX_HEIGHT_COORD = 1,
    HORZMAX_WIDTH_COORD = 2
};

// [min_contig, max_contig, min_size_percent, max_size_percent, min_height_percent, max_height_percent]
enum {
    MIN_CONTIG_COORD            = 0,
    MAX_CONTIG_COORD            = 1,
    MIN_SIZE_PERCENT_COORD      = 2,
    MAX_SIZE_PERCENT_COORD      = 3,
    MIN_HEIGHT_PERCENT_COORD    = 4,
    MAX_HEIGHT_PERCENT_COORD    = 5,
	MIN_WIDTH_PERCENT_COORD		= 6,
	MAX_WIDTH_PERCENT_COORD		= 7
};



extern const std::string OUTPUT_PARAM_BODY_HEIGHT = "body_height";
extern const std::string OUTPUT_PARAM_I_BODY_HEIGHT = "i_body_height";

char suggest_sl_replacement(char orig_letter, GROUP_SCORES& group_scores, bool force_replace) {
	//find maximal value
	float max = 0;
	std::vector<char> maxScoreLetters;
	for (GROUP_SCORES::iterator it = group_scores.begin(); it != group_scores.end(); it++) {
        if (force_replace && (it->first == orig_letter)) {
            continue;
        }

		if (it->second > max) {
			max = it->second;
			maxScoreLetters.clear();
		}
		
		if (it->second == max) {
			maxScoreLetters.push_back(it->first);
		}
	}

	print_sl_results(group_scores);

	if (group_scores.size() == 0) {
		// Tests failed (no connected components?)
		return orig_letter;
	
	} else if ((!force_replace) && ((max < 0.6) || (((max - group_scores[orig_letter]) < 0.1) && (orig_letter!='\\')))) {
		return orig_letter;
		
	} else {
		
		char result;
		
		if (maxScoreLetters.size() == 1) {
			result = maxScoreLetters[0];
			
		} else if (group_scores['l'] == max) {
			result = 'l';
			
		} else {
			//getchar();
			result = maxScoreLetters[0];
			float max_freq = -1;
			char  max_freq_letter = '?';
			for (size_t i = 0; i < maxScoreLetters.size(); ++i) {
				if (get_letter_frequency(maxScoreLetters[i]) > max_freq) {
					max_freq = get_letter_frequency(maxScoreLetters[i]);
					max_freq_letter = maxScoreLetters[i];
				}
			}
			
			DebugLog("UNEXPECTED max group! Returning [%c] having the highest frequency.", max_freq_letter);
			result = max_freq_letter;
		}
		
		DebugLog("MISMATCH! CHOSE: %c", result);
		return result;
	}
	
}

bool SortWidthPredicate(const vector<float> d1, vector<float> d2) {
	return d1[HORZMAX_WIDTH_COORD] < d2[HORZMAX_WIDTH_COORD];
}

CCTYPE_LIST get_big_component_dimensions(cimage& ci, float* outWidth, float* outHeight) {
    CCTYPE_LIST result = ci.analyzecomp(true);
    if (result.size() <= 1) {
        if (outWidth) { *outWidth = 0; }
        if (outHeight) { *outHeight = 0; }
    } else {
        int max_h = result[1][2];
        int min_h = result[1][0];
        int max_w = result[1][3];
        int min_w = result[1][1];

        if (outWidth) { *outWidth = max_w-min_w+1; }
        if (outHeight) { *outHeight = max_h-min_h+1; }
    }
    return result;
}

bool get_openings(cimage& ci, int side, float start_pos, float end_pos, bool bounds_required, float letter_thickness, 
        float* outWidth, float* outHeight, 
        float* outMaxDepth, float* outAverageWidth, float* outOpeningWidth, float* outMaxWidth, float* outArea) {
    CCTYPE_LIST result = get_big_component_dimensions(ci, outWidth, outHeight);

    if (result.size() <= 1) {
        return false;
    } 
    
    float mainComponentSizePercent = (((float)result[1][SIZE_COORD]) / ((float)result[0][SIZE_COORD]));
    if (mainComponentSizePercent < 0.75) {
        DebugLog("ERROR: Main component size is %.0f%% of all components combined (less than 75%%). Not running openings test.", mainComponentSizePercent * 100);
        return false;
    }

    
    float width, maxdepth, openingwidth, maxwidth, area;
    ci.old_openings_test(result[1], start_pos, end_pos, bounds_required, letter_thickness,
           (side == GET_OPENINGS_TOP) || (side == GET_OPENINGS_BOTTOM),
           ((side == GET_OPENINGS_TOP)||(side==GET_OPENINGS_LEFT))? 1 : -1,
           maxdepth, width, openingwidth, maxwidth, area);

    //new code
    /*float max_depth_coord, min_width, winwidth_start, minwidth_end, minwidth_depth;
    ci.openings_test(true, result[1], start_pos, end_pos, 
            bounds_required ? cimage::Bound : cimage::Any, 
            bounds_required ? cimage::Bound : cimage::Any, 
            1.0, //letter_thickness,
            (side == GET_OPENINGS_TOP) || (side == GET_OPENINGS_BOTTOM), 
            ((side == GET_OPENINGS_TOP)||(side==GET_OPENINGS_LEFT))? 1 : -1, 
            maxdepth, width, maxwidth, area, 
            max_depth_coord, min_width, winwidth_start, minwidth_end, minwidth_depth);*/
    if (outMaxDepth) { *outMaxDepth = maxdepth; }
    if (outAverageWidth) { *outAverageWidth = width; }
    if (outOpeningWidth) { *outOpeningWidth = openingwidth; }
    if (outMaxWidth) { *outMaxWidth = maxwidth; }
    if (outArea) { *outArea = area; }

    DebugLog("max_depth: %.2f, avg_width: %.2f, max_width: %.2f, area: %.2f, opening_width: %.2f", maxdepth, width, maxwidth, area, openingwidth);
    return true;
}

/*
void get_openings(cimage& ci, float letter_thickness,
        float* outWidth, float* outHeight,
        float* outTopDepth, float* outTopWidth, float* outTopOpeningWidth,
        float* outBottomDepth, float* outBottomWidth, float* outBottomOpeningWidth,
        float* outLeftDepth, float* outLeftWidth, float* outLeftOpeningWidth,
        float* outRightDepth, float* outRightWidth, float* outRightOpeningWidth ) {
    
    CCTYPE_LIST result = ci.analyzecomp(true);
    if (result.size() <= 1) {
        if (outWidth) { *outWidth = 0; }
        if (outHeight) { *outHeight = 0; }
        return;
    }
    int max_h = result[1][2];
    int min_h = result[1][0];
    int max_w = result[1][3];
    int min_w = result[1][1];

    if (outWidth) { *outWidth = max_w-min_w+1; }
    if (outHeight) { *outHeight = max_h-min_h+1; }

    float width, depth, openingwidth;

    if (outTopDepth || outTopWidth || outTopOpeningWidth) {
        ci.openings_test(result[1], letter_thickness, true, 1, depth, width, openingwidth);
        if (outTopDepth) { *outTopDepth = depth; }
        if (outTopWidth) { *outTopWidth = width; }
        if (outTopOpeningWidth) { *outTopOpeningWidth = openingwidth; }
    }
    if (outBottomDepth || outBottomWidth || outBottomOpeningWidth) {
        ci.openings_test(result[1], letter_thickness, true, -1, depth, width, openingwidth);
        if (outBottomDepth) { *outBottomDepth = depth; }
        if (outBottomWidth) { *outBottomWidth = width; }
        if (outBottomOpeningWidth) { *outBottomOpeningWidth = openingwidth; }
    }
    if (outLeftDepth || outLeftWidth || outLeftOpeningWidth) {
        ci.openings_test(result[1], letter_thickness, false, 1, depth, width, openingwidth);
        if (outLeftDepth) { *outLeftDepth = depth; }
        if (outLeftWidth) { *outLeftWidth = width; }
        if (outLeftOpeningWidth) { *outLeftOpeningWidth = openingwidth; }
    }
    if (outRightDepth || outRightWidth || outRightOpeningWidth) {
        ci.openings_test(result[1], letter_thickness, false, -1, depth, width, openingwidth);
        if (outRightDepth) { *outRightDepth = depth; }
        if (outRightWidth) { *outRightWidth = width; }
        if (outRightOpeningWidth) { *outRightOpeningWidth = openingwidth; }
    }

}
*/

SL_RESULTS get_single_letter_scores(char letter, cimage& ci) {
#if DEBUG_LOG
	ci.debug_print();
#endif

    if (letter == '1') {
        SL_RESULTS sl_results;
    
        if (ci.top_horiz_bar_test()) {
            DebugLog("MM replacing: it's a 7!!!");
            sl_results.group_scores['7'] = 1;
        } else {
            sl_results.group_scores['1'] = 1;
        }
        return sl_results;
    }
    
	// Analyze connected components
	CCTYPE_LIST result = ci.analyzecomp(true);
	
    // Are there any CC? (other than the summary line)
    if (result.size() <= 1) {
        return SL_RESULTS();
    }

	//the center of the "line tests" should be in the center of the main letter
	float h_center, w_center;    	
	if (result.size() > 1) {
		h_center = 100*float(result[1][0]+result[1][2])/2/ci.m_height;
		w_center = 100*float(result[1][1]+result[1][3])/2/ci.m_width;
	} else {
		h_center = 50;
		w_center = 50;
	}
	
	// run line tests
	int n_parts_h = (int)(ci.linetest_horizontal(h_center, 5).size());
	int n_parts_v = (int)(ci.linetest_vertical(w_center, 5).size());
	
	// run connected components on inverse image
	cimage* i_ci = ci.inverse();
	//i_ci->debug_print();
	CCTYPE_LIST i_result = i_ci->analyzecomp(false, result[0][4]);
	
	DebugLog("%c: robust %ld inv-robust: %ld  h-parts: %d v-parts: %d", letter, result.size()-1, i_result.size()-1, n_parts_h, n_parts_v);
	
	delete i_ci;


    //bottom right corner test
    float brc = ci.bottom_right_corner_test(result[1]);
    float trc = ci.top_right_corner_test(result[1]);
    float flat_right = ci.flat_right_test(result[1]);
#if SINGLELETTER_LOG    
    cout <<  "BRC: "<< dec << (brc*100) << "%"<<endl;
    cout <<  "TRC: "<< dec << (trc*100) << "%"<<endl;
    cout <<  "FLAT RIGHT: " << dec << (flat_right*100) << "%" << endl;
#endif    

    HORZTEST_TYPE horz = ci.horz_integration_test(result[1]);
#if SINGLELETTER_LOG 
    cout << "horz_test:" << horz[0] << "," << horz[1] <<", "<< horz[2] << "," << horz[3] << ", " << horz[4] << "," << horz[5] << "," << horz[6] << "," << horz[7] << endl;
#endif    
    
    HORZTESTMAX_TYPE horzmax = ci.horz_integration_maxtest(result[1], 1.45); //1.45
    //debug print
#if SINGLELETTER_LOG
    cout << "HORZ_TEST_MAX:" << endl;
    for(size_t i=0; i<horzmax.size(); i++) {
        cout << "size: "<<horzmax[i][0]<< ", height: "<<horzmax[i][1]<< ", width: " << horzmax[i][2] << endl;
    }
#endif    
    
    //filter out a maximas if it is 50%*median less wide than the others
    if (horzmax.size() > 2) {
        HORZTESTMAX_TYPE horz_temp(horzmax);
        sort(horz_temp.begin(), horz_temp.end(), SortWidthPredicate);
        //while the smallest element is 100%*median smaller than the largest element
        while ((horzmax.size()>2) && ((*(horz_temp.end()-1))[HORZMAX_WIDTH_COORD] - (*horz_temp.begin())[HORZMAX_WIDTH_COORD] > 0.5)) { 
            //cout << "ERASING..." <<endl;
            for (HORZTESTMAX_TYPE::iterator it = horzmax.begin(); it != horzmax.end(); it++) {
                if ((*it)[HORZMAX_HEIGHT_COORD] == horz_temp[0][HORZMAX_HEIGHT_COORD]) {
                    horzmax.erase(it);
                    break;
                }
            }
            horz_temp.erase(horz_temp.begin());
        }
    }
    DebugLog("CLEANING...");
    // if some maximas are close - merge them
    float last_height = -1;
    for (unsigned int i=0; i<horzmax.size(); i++) {
        DebugLog("difference: %f", (horzmax[i][HORZMAX_HEIGHT_COORD] - last_height));
        if (horzmax[i][HORZMAX_HEIGHT_COORD] - last_height < 0.14 ) {
            last_height = horzmax[i][HORZMAX_HEIGHT_COORD];
            horzmax[i-1][HORZMAX_HEIGHT_COORD] = (horzmax[i-1][HORZMAX_HEIGHT_COORD]+horzmax[i][HORZMAX_HEIGHT_COORD])/float(2);
            horzmax[i-1][HORZMAX_SIZE_COORD] = (horzmax[i-1][HORZMAX_SIZE_COORD]+horzmax[i][HORZMAX_SIZE_COORD]);
            horzmax[i-1][HORZMAX_WIDTH_COORD] = (horzmax[i-1][HORZMAX_WIDTH_COORD]+horzmax[i][HORZMAX_WIDTH_COORD])/float(2);
            horzmax.erase(horzmax.begin()+i);
            i--;
            DebugLog("ERASED_CLOSE...");
            /*if (horzmax[i][HORZMAX_WIDTH_COORD] < 1.7) {
                horzmax.erase(horzmax.begin()+i);
                i--;
                cout << "ERASED AGAIN..." <<endl;
            }*/
        } else {
            last_height = horzmax[i][HORZMAX_HEIGHT_COORD];
        }
    }

    DebugLog("HORZ_TEST_MAX:");
    for(size_t i=0; i<horzmax.size(); i++) {
        DebugLog("size: %f, height:%f, width: %f", horzmax[i][0], horzmax[i][1], horzmax[i][2]);
    }

    //if found two maximas that are close-by in the top or bottom - merge them
    if (horzmax.size() == 3) {
        if ((horzmax[0][HORZMAX_HEIGHT_COORD] < 0.25) && (horzmax[1][HORZMAX_HEIGHT_COORD] < 0.25)) {
            horzmax[0][HORZMAX_SIZE_COORD] = (horzmax[0][HORZMAX_SIZE_COORD] + horzmax[1][HORZMAX_SIZE_COORD] + horzmax[1][HORZMAX_HEIGHT_COORD] - horzmax[0][HORZMAX_HEIGHT_COORD]);
            horzmax[0][HORZMAX_WIDTH_COORD] = ((horzmax[0][HORZMAX_WIDTH_COORD]*horzmax[0][HORZMAX_SIZE_COORD]) + (horzmax[1][HORZMAX_WIDTH_COORD]*horzmax[1][HORZMAX_SIZE_COORD])) / (horzmax[0][HORZMAX_SIZE_COORD] + horzmax[1][HORZMAX_SIZE_COORD]);
            horzmax[0][HORZMAX_HEIGHT_COORD] = ((horzmax[0][HORZMAX_HEIGHT_COORD]*horzmax[0][HORZMAX_SIZE_COORD]) + (horzmax[1][HORZMAX_HEIGHT_COORD]*horzmax[1][HORZMAX_SIZE_COORD])) / (horzmax[0][HORZMAX_SIZE_COORD] + horzmax[1][HORZMAX_SIZE_COORD]);
            horzmax.erase(horzmax.begin()+1);
        } else if ((horzmax[1][HORZMAX_HEIGHT_COORD] > 0.75) && (horzmax[2][HORZMAX_HEIGHT_COORD] > 0.75)) {
            horzmax[1][HORZMAX_SIZE_COORD] = (horzmax[1][HORZMAX_SIZE_COORD] + horzmax[2][HORZMAX_SIZE_COORD] + horzmax[2][HORZMAX_HEIGHT_COORD] - horzmax[1][HORZMAX_HEIGHT_COORD]);
            horzmax[1][HORZMAX_WIDTH_COORD] = ((horzmax[1][HORZMAX_WIDTH_COORD]*horzmax[1][HORZMAX_SIZE_COORD]) + (horzmax[2][HORZMAX_WIDTH_COORD]*horzmax[2][HORZMAX_SIZE_COORD])) / (horzmax[1][HORZMAX_SIZE_COORD] + horzmax[2][HORZMAX_SIZE_COORD]);
            horzmax[1][HORZMAX_HEIGHT_COORD] = ((horzmax[1][HORZMAX_HEIGHT_COORD]*horzmax[1][HORZMAX_SIZE_COORD]) + (horzmax[2][HORZMAX_HEIGHT_COORD]*horzmax[2][HORZMAX_SIZE_COORD])) / (horzmax[1][HORZMAX_SIZE_COORD] + horzmax[2][HORZMAX_SIZE_COORD]);
            horzmax.erase(horzmax.begin()+2);
        }
    }

    DebugLog("size of horzmax is: %d", (unsigned short)horzmax.size());

    // Openings test
    float outWidth, outHeight;
    //float outTopDepth, outTopWidth, outTopOpeningWidth;
    //float outBottomDepth, outBottomWidth, outBottomOpeningWidth;
    //float outLeftDepth, outLeftWidth, outLeftOpeningWidth;
    float outRightDepth, outRightWidth, outRightOpeningWidth;

    bool successOpenings = get_openings(ci, GET_OPENINGS_RIGHT, 0.25,0.75, true,
            0.05 /* 5% */, 
            &outWidth, &outHeight, 
            &outRightDepth, &outRightWidth, &outRightOpeningWidth, NULL, NULL);

    if (!successOpenings) {
        return SL_RESULTS();
    }

    float right_side_opening_depth_percent = outRightDepth / outWidth; 
	
	//run the test
	//char letter = argv[fidx][0];

    if (is_in_il_group(letter)) {
        return il_group_scores(result, i_result, n_parts_h, n_parts_v, brc, trc, horz, horzmax, letter);
    } else if (is_in_oa_group(letter)) {
        return oa_group_scores(result, i_result, n_parts_h, n_parts_v, brc, trc, horz, flat_right, right_side_opening_depth_percent, letter);
    } else {
        return SL_RESULTS();
    }
}

bool is_in_il_group(char letter) {
	return (find(il_group.begin(), il_group.end(),letter) != il_group.end());
}

bool is_in_oa_group(char letter) {
	return (find(oa_group.begin(), oa_group.end(), letter) != oa_group.end());
}

float calc_score(std::vector<float> tests, std::vector<float>* weights = NULL, bool debug = false) {
    if ((weights != NULL) && (tests.size() != (*weights).size())) {
        cerr << "####### ERROR: SIZE MISMATCH!!! #########" << endl;
        weights = NULL;
    }

#if DEBUG_SINGLELETTER
    if (debug) {
        cout << "CALCSCORE:";
    }
#endif    
    float total = 0;
    float total_weights = 0;
    for (size_t i = 0; i < tests.size(); i++) {
		if ((tests[i] < -0.001) || (tests[i] > 1.001)) {
			cerr << "####### ERROR: SCORE VALUE OUT OF RANGE!!! #########" << endl;
			tests[i] = 0;
		}

#if DEBUG_SINGLELETTER        
        if (debug) {
            cout << tests[i];
        }
#endif        
		
        float w = (weights == NULL) ? 1.0 : (*weights)[i];
        total += (tests[i] * w);
        total_weights += w;
    }
#if DEBUG_SINGLELETTER    
    if (debug) {
        cout << endl;
    }
#endif    

    return total / total_weights;
}



#define NaN std::numeric_limits<float>::quiet_NaN()

float cc_y(CCTYPE_LIST& conn_comps, size_t idx) {
    if (idx >= conn_comps.size()) {
        return NaN;
    }

    return (conn_comps[idx][Y_MIN_COORD] + conn_comps[idx][Y_MAX_COORD]) / 2;
}
        
float cc_y_percent(CCTYPE_LIST& conn_comps, CCTYPE& ref_comp, size_t idx) {
    return (cc_y(conn_comps, idx) - ref_comp[Y_MIN_COORD]) / (ref_comp[Y_MAX_COORD] - ref_comp[Y_MIN_COORD]);
}

float cc_x(CCTYPE_LIST& conn_comps, size_t idx) {
    if (idx >= conn_comps.size()) {
        return NaN;
    }

    return (conn_comps[idx][X_MIN_COORD] + conn_comps[idx][X_MAX_COORD]) / 2;
}
        
float cc_x_percent(CCTYPE_LIST& conn_comps, CCTYPE& ref_comp, size_t idx) {
    return (cc_x(conn_comps, idx) - ref_comp[X_MIN_COORD]) / (ref_comp[X_MAX_COORD] - ref_comp[X_MIN_COORD]);
}

float cc_size(CCTYPE_LIST& conn_comps, size_t idx) {
    if (idx >= conn_comps.size()) {
        return NaN;
    }

    return conn_comps[idx][SIZE_COORD];
}
        
float cc_size_percent(CCTYPE_LIST& conn_comps, CCTYPE& ref_comp, size_t idx) {
    return cc_size(conn_comps, idx) / ref_comp[SIZE_COORD];
}

#define MAKE_VECTOR(a, T) std::vector<T>(a, a+(sizeof(a)/sizeof(a[0])))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) > 0) ? (a) : ((a)*(-1)))
#define IS_BETWEEN(a, low, high) (((low) <= (a)) && ((a) <= (high)))

bool is_cornerish(float corner_percent, float upper_limit = 0.15) {
    return ((corner_percent >= 0) && (corner_percent < upper_limit));
}

bool is_open(float open_depth_percent) {
    return ((open_depth_percent >= 0) && (open_depth_percent > 0.5));
}

bool is_not_open(float open_depth_percent) {
    return (open_depth_percent < 0.2);
}

const char il_group_array[] = {'i','l','!','I','t','f','|','\\'};
const size_t il_group_size = sizeof(il_group_array)/sizeof(il_group_array[0]);
const std::vector<char> il_group(il_group_array, il_group_array+il_group_size);

template<class T>
void print_vector(std::vector<T> v) {
#if DEBUG    
	cout << "[";
	for (size_t i = 0; i < v.size(); ++i) {
		if (i != 0) { cout << ","; };
		cout << v[i];
	}
	cout << "]";
#endif
}

SL_RESULTS il_group_scores(
    CCTYPE_LIST& conn_comps, CCTYPE_LIST& inv_conn_comps, 
    int horz_cuts, int vert_cuts, 
    float bottom_right_corner_height_percent, float top_right_corner_height_percent,
    HORZTEST_TYPE& horz_test,
    HORZTESTMAX_TYPE& horzmax_test,
    char ocr_letter) {

    int len_conn_comps = (int)conn_comps.size() - 1;
    CCTYPE_LIST& cc = conn_comps;

    float width = (cc[1][X_MAX_COORD]-cc[1][X_MIN_COORD]+1);
    float height = (cc[1][Y_MAX_COORD]-cc[1][Y_MIN_COORD]+1);
	float looks_like_i_must_test = IS_BETWEEN(horz_test[MIN_HEIGHT_PERCENT_COORD], 0.1, 0.25);
    float looks_like_i_tests[] = {
		(float)IS_BETWEEN(horz_test[MIN_HEIGHT_PERCENT_COORD], 0.1, 0.25),
        horz_test[MIN_CONTIG_COORD],
		(float)(horz_test[MIN_SIZE_PERCENT_COORD] < 0.15),
		(float)(horz_test[MIN_WIDTH_PERCENT_COORD] < 0.65),
        (float)IS_BETWEEN(float((horz_test[MIN_HEIGHT_PERCENT_COORD]*height)-width)/width, -0.3, 0.3)
	};
#if DEBUG_SINGLELETTER
    cout << "elongated:" << (float((horz_test[MIN_HEIGHT_PERCENT_COORD]*height)-width)/width) << endl;
#endif    
	float looks_like_i = looks_like_i_must_test * calc_score(MAKE_VECTOR(looks_like_i_tests, float));
#if DEBUG_SINGLELETTER    
    cout << "LOOKS LIKE I = " << looks_like_i << endl;
#endif    
	
	float looks_like_i_weak_must_test = IS_BETWEEN(horz_test[MIN_HEIGHT_PERCENT_COORD], 0.1, 0.35);
    float looks_like_i_weak_tests[] = {
		(float)IS_BETWEEN(horz_test[MIN_HEIGHT_PERCENT_COORD], 0.1, 0.3),
        horz_test[MIN_CONTIG_COORD],
	    (float)(horz_test[MIN_SIZE_PERCENT_COORD] < 0.15),
		(float)(horz_test[MIN_WIDTH_PERCENT_COORD] < 0.9)
	};
	float looks_like_i_weak = looks_like_i_weak_must_test * calc_score(MAKE_VECTOR(looks_like_i_weak_tests, float), NULL, true);

	float looks_like_excl_must_test = IS_BETWEEN(horz_test[MIN_HEIGHT_PERCENT_COORD], 0.9, 0.7);
    float looks_like_excl_tests[] = {
		horz_test[MIN_CONTIG_COORD],
		(float)(horz_test[MIN_SIZE_PERCENT_COORD] < 0.1)
	};
	float looks_like_excl = looks_like_excl_must_test * calc_score(MAKE_VECTOR(looks_like_excl_tests, float));

    float looks_like_t;
    if (horzmax_test.size() == 0) {
        looks_like_t = 0;
    } else {
        float looks_like_t1_must_test = (horzmax_test.size() == 1) && (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.12, 0.45));
        if ((ocr_letter != 't') && (ocr_letter != 'f')) {
            looks_like_t1_must_test *= ((horzmax_test.size() == 1) && (horzmax_test[0][HORZMAX_WIDTH_COORD] > 1.7));
        }
        float looks_like_t1_tests[] = {
            (float)(horzmax_test.size() == 1),
            (float)(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.12, 0.45)),
            (float)(horzmax_test[0][HORZMAX_SIZE_COORD] < 0.25)
	    };
        float looks_like_t1 = looks_like_t1_must_test * calc_score(MAKE_VECTOR(looks_like_t1_tests, float));
        float looks_like_t2_must_test = (horzmax_test.size() == 2) && (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.12, 0.45)) && (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.65, 1));
        if ((ocr_letter != 't') && (ocr_letter != 'f')) {
            looks_like_t2_must_test *= ((horzmax_test.size() == 2) && (horzmax_test[0][HORZMAX_WIDTH_COORD] > 1.7));
        }
        float looks_like_t2_tests[] = {
            (float)(horzmax_test.size() == 2),
            (float)(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.12, 0.45)),
            (float)(horzmax_test[0][HORZMAX_SIZE_COORD] < 0.25),
            (float)((horzmax_test.size() == 2) && (horzmax_test[1][HORZMAX_SIZE_COORD] < 0.2)),
            (float)((horzmax_test.size() == 2) && IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.75, 1))
        };
        float looks_like_t2 = looks_like_t2_must_test * calc_score(MAKE_VECTOR(looks_like_t2_tests, float), NULL, true);
 
        looks_like_t = MAX(looks_like_t1, looks_like_t2);
#if DEBUG_LOG
        cout << "LOOKS LIKE T = " << looks_like_t << endl;
#endif        
	}

    float looks_like_f;
    if (horzmax_test.size() == 0) {
        looks_like_f = 0;
    } else {
        float looks_like_f1_must_test = (horzmax_test.size() == 1) && (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.2, 0.55));
        float looks_like_f1_tests[] = {
            (float)(horzmax_test.size() == 1),
            (float)(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.2, 0.45)),
            (float)(horzmax_test[0][HORZMAX_SIZE_COORD] < 0.25)
        };
        float looks_like_f1 = looks_like_f1_must_test * calc_score(MAKE_VECTOR(looks_like_f1_tests, float));
        float looks_like_f2_must_test = (horzmax_test.size() == 2) && ( ((IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.55)) && (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.25))) || ((IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.2, 0.55)) && (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.9, 1))) ) ;
        //cout << "f2must:" <<(horzmax_test.size() == 2)<<";"<< (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.55)) <<";"<<(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.25)) << "----" << horzmax_test[1][HORZMAX_HEIGHT_COORD] << " " <<  horzmax_test[0][HORZMAX_HEIGHT_COORD] << endl;
        float looks_like_f2_tests[] = {
            (float)(horzmax_test.size() == 2),
            (float)((horzmax_test.size() == 2) && ((IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.49)) || (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0.2, 0.49)))),
            (float)((horzmax_test.size() == 2) && (horzmax_test[0][HORZMAX_SIZE_COORD] < 0.25) && (horzmax_test[1][HORZMAX_SIZE_COORD] < 0.25)),
            (float)(horzmax_test[0][HORZMAX_SIZE_COORD] < 0.2),
            (float)(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.2))
        };
        float looks_like_f2 = looks_like_f2_must_test * calc_score(MAKE_VECTOR(looks_like_f2_tests, float), NULL, true);
        float looks_like_f3_must_test = (horzmax_test.size() == 3) && (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.55)) && (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.25)) && (IS_BETWEEN(horzmax_test[2][HORZMAX_HEIGHT_COORD], 0.9, 1));
        //cout << "f3MUST " << horzmax_test.size() <<";"<<(horzmax_test.size() == 3) << ";" << (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.55)) << ";" << (IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.25)) << ";" << (IS_BETWEEN(horzmax_test[2][HORZMAX_HEIGHT_COORD], 0.9, 1)) << endl;
        float looks_like_f3_tests[] = {
            (float)(horzmax_test.size() == 3),
            (float)((horzmax_test.size() == 3) && (IS_BETWEEN(horzmax_test[1][HORZMAX_HEIGHT_COORD], 0.2, 0.49))),
            (float)((horzmax_test.size() == 3) && (horzmax_test[1][HORZMAX_SIZE_COORD] < 0.25)),
            (float)((horzmax_test.size() == 3) && (horzmax_test[2][HORZMAX_SIZE_COORD] < 0.25)),
            (float)(horzmax_test[0][HORZMAX_SIZE_COORD] < 0.2),
            (float)(IS_BETWEEN(horzmax_test[0][HORZMAX_HEIGHT_COORD], 0, 0.2)),
            (float)((horzmax_test.size() == 3) && (IS_BETWEEN(horzmax_test[2][HORZMAX_HEIGHT_COORD], 0.9, 1)))
        };
        float looks_like_f3 = looks_like_f3_must_test * calc_score(MAKE_VECTOR(looks_like_f3_tests, float));

        looks_like_f = MAX(MAX(looks_like_f1, looks_like_f2), looks_like_f3);
#if DEBUG_LOG        
        cout << "LOOKS LIKE F = " << looks_like_f << endl;
#endif        
    }
    //// LOWER-CASE L ////
    float score_l_tests[] = {
        (float)(len_conn_comps == 1), (float)(horz_cuts == 1), (float)(vert_cuts == 1),
        // Doesn't look like i or ! or t or f
        1 - ((ocr_letter == 'i')? looks_like_i_weak : looks_like_i), 1 - looks_like_excl, 1 - looks_like_t, 1 - looks_like_f
	};
    float score_l_weights_array[]   = {1,1,1,1,1,1,1}; //0.25,0.25,0.25,0.25};
    std::vector<float> score_l_weights = MAKE_VECTOR(score_l_weights_array, float);
    //score_l_weights = [...]
    float score_l = calc_score(MAKE_VECTOR(score_l_tests, float), &score_l_weights);

    //// LOWER-CASE I ////
	float score_i_non_contig_must_test = cc_y_percent(cc, cc[0], 1) > cc_y_percent(cc, cc[0], 2);
    float score_i_non_contig_tests[] = {
        (float)(len_conn_comps == 2),
        (float)(cc_size_percent(cc, cc[0], 1) > 0.6), //body
        (float)(cc_size_percent(cc, cc[0], 2) > 0.1), //dot
//        cc_size_percent(cc, cc[0], 2) > 0.02, //dot
//        ABS(cc_x_percent(cc, cc[0], 1) - cc_x_percent(cc, cc[0], 2)) < 0.1,
        (float)((cc_x_percent(cc, cc[0], 1) - cc_x_percent(cc, cc[0], 2)) > - 0.1),
//        horz_cuts == 1, vert_cuts == 2
        (float)(horz_cuts == 1), (float)(IS_BETWEEN(vert_cuts,1,2))
    };
    float score_i_contig_tests[] = {
        (float)(len_conn_comps == 1), (float)(horz_cuts == 1), (float)(IS_BETWEEN(vert_cuts, 1, 2)),
        (ocr_letter == 'i')? looks_like_i_weak : looks_like_i,
        //looks_like_i,
		1 - looks_like_t,
        1 - looks_like_f
    };
    float score_i_contig_weights_array[]   = {1,1,1,4,1,1};
    std::vector<float> score_i_contig_weights = MAKE_VECTOR(score_i_contig_weights_array, float);

    float score_i;
    float score_i_contig = calc_score(MAKE_VECTOR(score_i_contig_tests, float), &score_i_contig_weights);
    float score_i_noncontig = score_i_non_contig_must_test * calc_score(MAKE_VECTOR(score_i_non_contig_tests, float));

    score_i = MAX(score_i_contig, score_i_noncontig);

    //// EXCLAMATION MARK ////
    float score_excl_non_contig_tests[] = {
        (float)(len_conn_comps == 2),
        (float)(IS_BETWEEN(cc_size_percent(cc, cc[0], 1), 0.6, 0.9)), //body
        (float)(cc_y_percent(cc, cc[0], 1) < cc_y_percent(cc, cc[0], 2)),
        (float)(ABS(cc_x_percent(cc, cc[0], 1) - cc_x_percent(cc, cc[0], 2)) < 0.1),
        (float)(horz_cuts == 1),
        (float)(vert_cuts == 2)
    };
	/*cout << cc_size_percent(cc, cc[0], 1) << "," << cc_y_percent(cc, cc[0], 1) << "," << cc_y_percent(cc, cc[0], 2)
	<< "," << cc_x_percent(cc, cc[0], 1) << "," << cc_x_percent(cc, cc[0], 2) << endl;
	print_vector(MAKE_VECTOR(score_excl_non_contig_tests, float));*/
	
    float score_excl_contig_tests[] = {
        (float)(len_conn_comps == 1),
        (float)(horz_cuts == 1),
        (float)(vert_cuts == 1),
        looks_like_excl
    };
    float score_excl_contig_weights_array[]   = {1,1,1,4};
    std::vector<float> score_excl_contig_weights = MAKE_VECTOR(score_excl_contig_weights_array, float);
    float score_excl = MAX(
        calc_score(MAKE_VECTOR(score_excl_non_contig_tests, float)),
        calc_score(MAKE_VECTOR(score_excl_contig_tests, float), &score_excl_contig_weights));

    //// LOWER-CASE T ////
    float score_t_tests[] = {
        (float)(len_conn_comps == 1),
        (float)(horz_cuts == 1),
        (float)(IS_BETWEEN(vert_cuts, 1, 2)),
        looks_like_t, 
        (float)(is_cornerish(bottom_right_corner_height_percent, 0.1)),
        (float)(!is_cornerish(top_right_corner_height_percent, 0.1)),
        1 - looks_like_i
    };
    float score_t_weights_array[]   = {1,1,1,4,1,1,1};
    std::vector<float> score_t_weights = MAKE_VECTOR(score_t_weights_array, float);
    float score_t = calc_score(MAKE_VECTOR(score_t_tests, float), &score_t_weights);

    //// LOWER-CASE F ////
    float score_f_tests[] = {
        (float)(len_conn_comps == 1),
        (float)(horz_cuts == 1),
        (float)(IS_BETWEEN(vert_cuts, 1, 2)),
        looks_like_f, 
        (float)(!is_cornerish(bottom_right_corner_height_percent, 0.1)),
        (float)(is_cornerish(top_right_corner_height_percent, 0.1)),
        1 - looks_like_i
    };
    float score_f_weights_array[]   = {1,1,1,4,1,1,1};
    std::vector<float> score_f_weights = MAKE_VECTOR(score_f_weights_array, float);
    float score_f = calc_score(MAKE_VECTOR(score_f_tests, float), &score_f_weights);

    SL_RESULTS sl_results;

    sl_results.output_params[OUTPUT_PARAM_BODY_HEIGHT] = cc[1][Y_MAX_COORD] - cc[1][Y_MIN_COORD] + 1;
    if (score_i_contig > score_i_noncontig) {
        sl_results.output_params[OUTPUT_PARAM_I_BODY_HEIGHT] = sl_results.output_params[OUTPUT_PARAM_BODY_HEIGHT]*(1-horz_test[MIN_HEIGHT_PERCENT_COORD]);
    } else {
        sl_results.output_params[OUTPUT_PARAM_I_BODY_HEIGHT] = sl_results.output_params[OUTPUT_PARAM_BODY_HEIGHT];
    }
    
    sl_results.group_scores['l'] = score_l;
    sl_results.group_scores['i'] = score_i;
    sl_results.group_scores['!'] = score_excl;
    sl_results.group_scores['t'] = score_t;
    sl_results.group_scores['f'] = score_f;
    // TODO
    sl_results.group_scores['I'] = sl_results.group_scores['|'] = score_l;

    return sl_results;
}

const char oa_group_array[] = {'c', 'o'};
const size_t oa_group_size = sizeof(oa_group_array)/sizeof(oa_group_array[0]);
const std::vector<char> oa_group(oa_group_array, oa_group_array+oa_group_size);

SL_RESULTS oa_group_scores(
    CCTYPE_LIST& conn_comps, CCTYPE_LIST& inv_conn_comps, 
    int horz_cuts, int vert_cuts, 
    float bottom_right_corner_height_percent, float top_right_corner_height_percent,
    HORZTEST_TYPE& horz_test, float flat_right,
    float right_side_opening_depth_percent,
    char ocr_letter) {

    //int len_conn_comps = conn_comps.size() - 1;
    //int len_inv_conn_comps = inv_conn_comps.size();
    //CCTYPE_LIST& cc = conn_comps;
    //CCTYPE_LIST& inv_cc = inv_conn_comps;

    // CLOSED-C => O/A TEST
    if (((ocr_letter == 'c') && is_not_open(right_side_opening_depth_percent))       || (ocr_letter == 'o')) {

        float score_o_tests[] = {
            (float)(!(is_cornerish(bottom_right_corner_height_percent) || is_cornerish(top_right_corner_height_percent))),
            (float)(flat_right < 0.85)
        };
        float score_o = calc_score(MAKE_VECTOR(score_o_tests, float));

        float score_a_like_o_tests[] = {
            (float)(is_cornerish(bottom_right_corner_height_percent) || is_cornerish(top_right_corner_height_percent)),
            (float)(flat_right > 0.75)
        };
        float score_a_like_o = calc_score(MAKE_VECTOR(score_a_like_o_tests, float));

        if (score_o > 0 && score_a_like_o > 0 && score_o < 0.6 && score_a_like_o < 0.6) {
            score_o += 0.4;
            score_a_like_o += 0.4;
        }

        SL_RESULTS result;
        result.group_scores['o'] = score_o; 
        result.group_scores['a'] = score_a_like_o; 
        return result;
    }

    // DEFAULT: DO NOTHING
    return SL_RESULTS();
}

const char full_oa_group_array[] = {'o','a','c','e','@','u','O','C','U'};
const size_t full_oa_group_size = sizeof(full_oa_group_array)/sizeof(full_oa_group_array[0]);
const std::vector<char> full_oa_group(full_oa_group_array, full_oa_group_array+full_oa_group_size);

SL_RESULTS full_oa_group_scores(
    CCTYPE_LIST& conn_comps, CCTYPE_LIST& inv_conn_comps, 
    int horz_cuts, int vert_cuts, 
    float bottom_right_corner_height_percent, float top_right_corner_height_percent,
    HORZTEST_TYPE& horz_test, float flat_right) {

    int len_conn_comps = (int)conn_comps.size() - 1;
    int len_inv_conn_comps = (int)inv_conn_comps.size();
    CCTYPE_LIST& cc = conn_comps;
    CCTYPE_LIST& inv_cc = inv_conn_comps;

    //// o/O ////
    float score_o_tests[] = {
        (float)(len_conn_comps == 1),
        (float)(len_inv_conn_comps == 2),
        // to do no %?
        (float)(ABS(cc_x_percent(cc, cc[0], 1) - cc_x_percent(inv_cc, cc[0], 1)) < 0.1),
        (float)(ABS(cc_y_percent(cc, cc[0], 1) - cc_y_percent(inv_cc, cc[0], 1)) < 0.1),
        (float)(cc_size(inv_cc, 1) > 0.2*cc_size(cc, 1)),
        (float)(horz_cuts == 2),
        (float)(vert_cuts == 2),
        (float)(!(is_cornerish(bottom_right_corner_height_percent) || is_cornerish(top_right_corner_height_percent))),
        (float)(flat_right < 0.85)
    };
    // score_o_weights = [...]
    float score_o = calc_score(MAKE_VECTOR(score_o_tests, float));

    //// c/C ////
    float score_c_tests[] = {
        (float)(len_conn_comps == 1),
        (float)(len_inv_conn_comps == 1),
        (float)(horz_cuts == 1),
        (float)(vert_cuts == 2)
    };
    // score_c_weights = [...]
    float score_c = calc_score(MAKE_VECTOR(score_c_tests, float));

    //// u/U ////
    float score_u_tests[] = {
        (float)(len_conn_comps == 1), (float)(len_inv_conn_comps == 1),
        (float)(horz_cuts == 2), (float)(vert_cuts == 1)
    };
    // score_u_weights = [...]
    float score_u = calc_score(MAKE_VECTOR(score_u_tests, float));

    //// @ SIGN ////
    float score_at_tests[] = {
        (float)(len_conn_comps == 1), (float)(len_inv_conn_comps >= 2),
        (float)(horz_cuts == 4), (float)(vert_cuts == 4)
    };
    // score_at_weights = [...]
    float score_at = calc_score(MAKE_VECTOR(score_at_tests, float));

    //// LOWER-CASE E ////
    float score_e_tests[] = {
        (float)(len_conn_comps == 1), (float)(IS_BETWEEN(len_inv_conn_comps, 2, 3)),
        (float)(IS_BETWEEN(horz_cuts, 1, 2)), (float)(IS_BETWEEN(vert_cuts, 2, 3)),
        (float)(flat_right < 0.70)
    };
    std::vector<float> score_e_tests_vec = MAKE_VECTOR(score_e_tests, float);
    if (len_inv_conn_comps == 2) {
        //score_e_tests_vec.push_back(cc_y(cc, 1) > cc_y(inv_cc, 1));
        score_e_tests_vec.push_back(cc_y_percent(cc, cc[0], 1) - cc_y_percent(inv_cc, cc[0], 1) > 0.15);
        score_e_tests_vec.push_back(cc_size(inv_cc, 1) < 0.4*cc_size(cc, 1));
#if DEBUG        
        cout << "XXXX: " << (float)cc_size(inv_cc, 1)/(float)cc_size(cc, 1) << endl;
        cout << "YYYY: " << (cc_y(cc, 1) - cc_y(inv_cc, 1)) << endl;
        cout << "ZZZZ: " << (cc_y_percent(cc, cc[0], 1) - cc_y_percent(inv_cc, cc[0], 1)) << endl;
#endif        
    }
    // score_e_weights = [...]
    float score_e = calc_score(score_e_tests_vec);

    //// e-LIKE LOWER-CASE A ////
    float score_a_like_e_tests[] = {
        (float)(len_conn_comps == 1), (float)(IS_BETWEEN(len_inv_conn_comps, 2, 3)),
        (float)(IS_BETWEEN(horz_cuts, 1, 2)), (float)(IS_BETWEEN(vert_cuts, 2, 3)),
        (float)(cc_y(cc, 1) < cc_y(inv_cc, 1)),
        (float)(is_cornerish(bottom_right_corner_height_percent) || is_cornerish(top_right_corner_height_percent))
    };

    //// o-LIKE LOWER-CASE A ////
    float score_a_like_o_tests[] = {
        (float)(len_conn_comps == 1), (float)(len_inv_conn_comps == 2),
        // to do no %?
        (float)(ABS(cc_x_percent(cc, cc[0], 1) - cc_x_percent(inv_cc, cc[0], 1)) < 0.1),
        (float)(ABS(cc_y_percent(cc, cc[0], 1) - cc_y_percent(inv_cc, cc[0], 1)) < 0.1),
        (float)(cc_size(inv_cc, 1) > 0.15*cc_size(cc, 1)),
        (float)(horz_cuts == 2), (float)(vert_cuts == 2),
        (float)(is_cornerish(bottom_right_corner_height_percent) || is_cornerish(top_right_corner_height_percent)),
        (float)(flat_right > 0.75)
    };
#if DEBUG    
    cout << cc_size(inv_cc, 1) << "," << cc_size(cc, 1) << endl;   

    cout << "e-LIKE A: ";
    print_vector(MAKE_VECTOR(score_a_like_e_tests, float));
    cout << endl;
    cout << "o-LIKE A: ";
    print_vector(MAKE_VECTOR(score_a_like_o_tests, float));
    cout << endl;
#endif    

    float score_a = MAX(
        calc_score(MAKE_VECTOR(score_a_like_e_tests, float)),
        calc_score(MAKE_VECTOR(score_a_like_o_tests, float)));

    SL_RESULTS result;
    result.group_scores['o'] = result.group_scores['O'] = score_o; 
    result.group_scores['u'] = result.group_scores['U'] = score_u; 
    result.group_scores['c'] = result.group_scores['C'] = score_c; 
    result.group_scores['e'] = score_e; 
    result.group_scores['a'] = score_a;
    result.group_scores['@'] = score_at; 
    return result;
}

void print_sl_results(GROUP_SCORES scogroup_scores) {
#if DEBUG_SINGLELETTER   
    std::cout << "RESULTS:" << std::endl;
    for(GROUP_SCORES::iterator iter = scogroup_scores.begin(); iter != scogroup_scores.end(); iter++) {
        std::cout << iter->first << " = " << iter->second << std::endl;
    }
#endif    
}

float get_letter_frequency(char letter) {
    static float letter_frequencies[] = { 8.167, 1.492, 2.782, 4.253, 12.702, 2.228, 2.015, 6.094, 6.966, 0.153, 0.772, 4.025, 2.406, 6.749, 7.507, 1.929, 0.095, 5.987, 6.327, 9.056, 2.758, 0.978, 2.360, 0.150, 1.974, 0.074};
    if ((letter>='A') && (letter <='Z')) {
        letter = letter + ('a'-'A');
    }
    if ((letter>='a') && (letter <='z')) {
        return letter_frequencies[letter-'a'];
    } else {
        return 0;
    }
}

float get_o_like_a_score(cimage& ci) {
	// Analyze connected components
	CCTYPE_LIST result = ci.analyzecomp(true);
	
	//the center of the "line tests" should be in the center of the main letter
	float h_center, w_center;    	
	if (result.size() > 1) {
		h_center = 100*float(result[1][0]+result[1][2])/2/ci.m_height;
		w_center = 100*float(result[1][1]+result[1][3])/2/ci.m_width;
	} else {        
		DebugLog("NO CONNECTED COMPONENTS FOUND!");
		return -1.0;
	}
	
	// run line tests
	int n_parts_h = (int)(ci.linetest_horizontal(h_center, 5).size());
	int n_parts_v = (int)(ci.linetest_vertical(w_center, 5).size());
	
	// run connected components on inverse image
	cimage* i_ci = ci.inverse();
	CCTYPE_LIST i_result = i_ci->analyzecomp(false, result[0][4]);
#if DEBUG    
	cout << "o_like_a: robust " << result.size()-1 << " inv-robust: " << i_result.size()-1 << " h-parts: " << n_parts_h << " v-parts: " << n_parts_v <<endl;
#endif    
	delete i_ci;
	
	if (i_result.size() > 2) {
		DebugLog("TOO MANY HOLES FOUND!");
		return -1.0;
	}
	
	CCTYPE_LIST& cc = result;
	CCTYPE_LIST& inv_cc = i_result;
	
    // Openings test
    float outWidth, outHeight;
    //float outTopDepth, outTopWidth, outTopOpeningWidth;
    //float outBottomDepth, outBottomWidth, outBottomOpeningWidth;
    float outLeftDepth, outLeftWidth, outLeftOpeningWidth;
    //float outRightDepth, outRightWidth, outRightOpeningWidth;
	
    bool successOpenings = get_openings(ci, GET_OPENINGS_LEFT, 0.05, 0.5, true, 0.05 /* 5% */, 
				 &outWidth, &outHeight, 
				 &outLeftDepth, &outLeftWidth, &outLeftOpeningWidth, NULL, NULL);
                 
    if (!successOpenings) {
        return -1.0;
    }
	
    float left_side_opening_depth_percent = outLeftDepth / outWidth; 
	
    //// e-LIKE LOWER-CASE A ////
    float score_a_like_e_tests[] = {
		// 3 vertical cuts
		(float)(n_parts_v == 3),
		// non-centricity of hole		
        (float)(cc_y(cc, 1) < cc_y(inv_cc, 1)),
		// open on left
		(float)(left_side_opening_depth_percent > 0.3)
    };
	
#if DEBUG    
    cout << "e-LIKE A: ";
    print_vector(MAKE_VECTOR(score_a_like_e_tests, float));
    cout << endl;
#endif     
	
    return 1 - calc_score(MAKE_VECTOR(score_a_like_e_tests, float));
}
