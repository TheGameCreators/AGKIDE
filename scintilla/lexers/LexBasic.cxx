// Scintilla source code edit control
/** @file LexBasic.cxx
 ** Lexer for BlitzBasic and PureBasic.
 ** Converted to lexer object and added further folding features/properties by "Udo Lechner" <dlchnr(at)gmx(dot)net>
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

// This tries to be a unified Lexer/Folder for all the BlitzBasic/BlitzMax/PurBasic basics
// and derivatives. Once they diverge enough, might want to split it into multiple
// lexers for more code clearity.
//
// Mail me (elias <at> users <dot> sf <dot> net) for any bugs.

// Folding only works for simple things like functions or types.

// You may want to have a look at my ctags lexer as well, if you additionally to coloring
// and folding need to extract things like label tags in your editor.

// Modified to work for AGK

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include <string>
#include <map>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"
#include "OptionSet.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

/* Bits:
 * 1  - whitespace
 * 2  - operator
 * 4  - identifier
 * 8  - decimal digit
 * 16 - hex digit
 * 32 - bin digit
 */
static int character_classification[128] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  0,  0,  1,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  2,  0,  4,  4,  2,  2,  0,  2,  2,  2,  2,  2,  2,  10, 2,
    60, 60, 28, 28, 28, 28, 28, 28, 28, 28, 2,  2,  2,  2,  2,  2,
    2,  20, 20, 20, 20, 20, 20, 4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  4,
    2,  20, 20, 20, 20, 20, 20, 4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  0
};

static bool IsSpace(int c) {
	return c < 128 && (character_classification[c] & 1);
}

static bool IsOperator(int c) {
	return c < 128 && (character_classification[c] & 2);
}

static bool IsIdentifier(int c) {
	return c < 128 && (character_classification[c] & 4);
}

static bool IsDigit(int c) {
	return c < 128 && (character_classification[c] & 8);
}

static bool IsHexDigit(int c) {
	return c < 128 && (character_classification[c] & 16);
}

static bool IsBinDigit(int c) {
	return c < 128 && (character_classification[c] & 32);
}

static int LowerCase(int c)
{
	if (c >= 'A' && c <= 'Z')
		return 'a' + c - 'A';
	return c;
}

static int CheckAGKFoldPoint(char const *token, int &level) {
	if (!strcmp(token, "function") 
	 || !strcmp(token, "type")
	 || !strcmp(token, "do")
	 || !strcmp(token, "while")
	 || !strcmp(token, "repeat")
	 || !strcmp(token, "for")
	 || !strcmp(token, "foldstart")
	 || !strcmp(token, "select")
	 || !strcmp(token, "case")) 
	{
		level |= SC_FOLDLEVELHEADERFLAG;
		return 1;
	}
	if (!strcmp(token, "endfunction") 
	 || !strcmp(token, "endtype")
	 || !strcmp(token, "loop")
	 || !strcmp(token, "endwhile")
	 || !strcmp(token, "until")
	 || !strcmp(token, "next")
	 || !strcmp(token, "foldend")
	 || !strcmp(token, "endselect")
	 || !strcmp(token, "endcase")) 
	{
		return -1;
	}
	return 0;
}

// An individual named option for use in an OptionSet

// Options used for LexerBasic
struct OptionsBasic {
	bool fold;
	bool foldSyntaxBased;
	bool foldCommentExplicit;
	std::string foldExplicitStart;
	std::string foldExplicitEnd;
	bool foldExplicitAnywhere;
	bool foldCompact;
	OptionsBasic() {
		fold = false;
		foldSyntaxBased = true;
		foldCommentExplicit = false;
		foldExplicitStart = "";
		foldExplicitEnd   = "";
		foldExplicitAnywhere = false;
		foldCompact = true;
	}
};

static const char * const agkWordListDesc[] = {
	"AGK Keywords",
	"AGK PreProcessor Keywords",
	"user defined 1",
	"user defined 2",
	0
};

struct OptionSetBasic : public OptionSet<OptionsBasic> {
	OptionSetBasic(const char * const wordListDescriptions[]) {
		DefineProperty("fold", &OptionsBasic::fold);

		DefineProperty("fold.basic.syntax.based", &OptionsBasic::foldSyntaxBased,
			"Set this property to 0 to disable syntax based folding.");

		DefineProperty("fold.basic.comment.explicit", &OptionsBasic::foldCommentExplicit,
			"This option enables folding explicit fold points when using the Basic lexer. "
			"Explicit fold points allows adding extra folding by placing a ;{ (BB/PB) or '{ (FB) comment at the start "
			"and a ;} (BB/PB) or '} (FB) at the end of a section that should be folded.");

		DefineProperty("fold.basic.explicit.start", &OptionsBasic::foldExplicitStart,
			"The string to use for explicit fold start points, replacing the standard ;{ (BB/PB) or '{ (FB).");

		DefineProperty("fold.basic.explicit.end", &OptionsBasic::foldExplicitEnd,
			"The string to use for explicit fold end points, replacing the standard ;} (BB/PB) or '} (FB).");

		DefineProperty("fold.basic.explicit.anywhere", &OptionsBasic::foldExplicitAnywhere,
			"Set this property to 1 to enable explicit fold points anywhere, not just in line comments.");

		DefineProperty("fold.compact", &OptionsBasic::foldCompact);

		DefineWordListSets(wordListDescriptions);
	}
};

class LexerBasic : public ILexer {
	char comment_char;
	int (*CheckFoldPoint)(char const *, int &);
	WordList keywordlists[4];
	OptionsBasic options;
	OptionSetBasic osBasic;
public:
	LexerBasic(char comment_char_, int (*CheckFoldPoint_)(char const *, int &), const char * const wordListDescriptions[]) :
	           comment_char(comment_char_),
	           CheckFoldPoint(CheckFoldPoint_),
	           osBasic(wordListDescriptions) {
	}
	virtual ~LexerBasic() {
	}
	void SCI_METHOD Release() {
		delete this;
	}
	int SCI_METHOD Version() const {
		return lvOriginal;
	}
	const char * SCI_METHOD PropertyNames() {
		return osBasic.PropertyNames();
	}
	int SCI_METHOD PropertyType(const char *name) {
		return osBasic.PropertyType(name);
	}
	const char * SCI_METHOD DescribeProperty(const char *name) {
		return osBasic.DescribeProperty(name);
	}
	int SCI_METHOD PropertySet(const char *key, const char *val);
	const char * SCI_METHOD DescribeWordListSets() {
		return osBasic.DescribeWordListSets();
	}
	int SCI_METHOD WordListSet(int n, const char *wl);
	void SCI_METHOD Lex(unsigned int startPos, int length, int initStyle, IDocument *pAccess);
	void SCI_METHOD Fold(unsigned int startPos, int length, int initStyle, IDocument *pAccess);

	void * SCI_METHOD PrivateCall(int, void *) {
		return 0;
	}
	static ILexer *LexerFactoryAGKBasic() {
		return new LexerBasic('`', CheckAGKFoldPoint, agkWordListDesc );
	}
};

int SCI_METHOD LexerBasic::PropertySet(const char *key, const char *val) {
	if (osBasic.PropertySet(&options, key, val)) {
		return 0;
	}
	return -1;
}

int SCI_METHOD LexerBasic::WordListSet(int n, const char *wl) {
	WordList *wordListN = 0;
	switch (n) {
	case 0:
		wordListN = &keywordlists[0];
		break;
	case 1:
		wordListN = &keywordlists[1];
		break;
	case 2:
		wordListN = &keywordlists[2];
		break;
	case 3:
		wordListN = &keywordlists[3];
		break;
	}
	int firstModification = -1;
	if (wordListN) {
		WordList wlNew;
		wlNew.Set(wl);
		if (*wordListN != wlNew) {
			wordListN->Set(wl);
			firstModification = 0;
		}
	}
	return firstModification;
}

void SCI_METHOD LexerBasic::Lex(unsigned int startPos, int length, int initStyle, IDocument *pAccess) {
	LexAccessor styler(pAccess);

	bool wasfirst = true, isfirst = true; // true if first token in a line
	styler.StartAt(startPos);

	StyleContext sc(startPos, length, initStyle, styler);

	// Can't use sc.More() here else we miss the last character
	for (; ; sc.Forward()) {
		if (sc.state == SCE_B_IDENTIFIER) {
			if (!IsIdentifier(sc.ch)) {
				// Labels
				if (wasfirst && sc.Match(':')) {
					sc.ChangeState(SCE_B_LABEL);
					sc.ForwardSetState(SCE_B_DEFAULT);
				} else {
					char s[100];
					int kstates[4] = {
						SCE_B_KEYWORD,
						SCE_B_KEYWORD2,
						SCE_B_KEYWORD3,
						SCE_B_KEYWORD4,
					};
					sc.GetCurrentLowered(s, sizeof(s));
					for (int i = 0; i < 4; i++) {
						if (keywordlists[i].InList(s)) {
							sc.ChangeState(kstates[i]);
						}
					}
					// Types, must set them as operator else they will be
					// matched as number/constant
					if (sc.Match('.') || sc.Match('$') || sc.Match('%') ||
						sc.Match('#')) {
						sc.SetState(SCE_B_OPERATOR);
					} else {
						sc.SetState(SCE_B_DEFAULT);
					}
				}
			}
		} else if (sc.state == SCE_B_OPERATOR) {
			if (!IsOperator(sc.ch) || sc.Match('#'))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_LABEL) {
			if (!IsIdentifier(sc.ch))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_CONSTANT) {
			if (!IsIdentifier(sc.ch))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_NUMBER) {
			if (!IsDigit(sc.ch))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_HEXNUMBER) {
			if (!IsHexDigit(sc.ch))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_BINNUMBER) {
			if (!IsBinDigit(sc.ch))
				sc.SetState(SCE_B_DEFAULT);
		} else if (sc.state == SCE_B_STRING) {
			if (sc.ch == '"') {
				sc.ForwardSetState(SCE_B_DEFAULT);
			}
			if (sc.atLineEnd) {
				sc.ChangeState(SCE_B_ERROR);
				sc.SetState(SCE_B_DEFAULT);
			}
		} else if (sc.state == SCE_B_STRING2) {
			if (sc.ch == '\'' && sc.chPrev != '\\') {
				sc.ForwardSetState(SCE_B_DEFAULT);
			}
			if (sc.atLineEnd) {
				sc.ChangeState(SCE_B_ERROR);
				sc.SetState(SCE_B_DEFAULT);
			}
		} else if (sc.state == SCE_B_COMMENT || sc.state == SCE_B_PREPROCESSOR) {
			if (sc.atLineEnd) {
				sc.SetState(SCE_B_DEFAULT);
			}
		} else if (sc.state == SCE_B_COMMENT_BLOCK) {
			if (sc.Match('*','/')) {
				sc.Forward();
				sc.ForwardSetState(SCE_B_DEFAULT);
			} else if (sc.MatchIgnoreCase("remend")) {
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.ForwardSetState(SCE_B_DEFAULT);
			}
		} 

		if (sc.atLineStart)
			isfirst = true;

		if (sc.state == SCE_B_DEFAULT || sc.state == SCE_B_ERROR) {
			if (isfirst && sc.Match('.')) {
				sc.SetState(SCE_B_LABEL);
			} else if (isfirst && sc.Match('#')) {
				wasfirst = isfirst;
				sc.SetState(SCE_B_IDENTIFIER);
			} else if (sc.Match('/', '/')) {
				sc.SetState(SCE_B_COMMENT);
				sc.Forward();
			} else if (sc.Match("`")) {
				sc.SetState(SCE_B_COMMENT);
			} else if (sc.Match('/', '*')) {
				sc.SetState(SCE_B_COMMENT_BLOCK);
				sc.Forward();
			} else if (sc.MatchIgnoreCase("remstart")) {
				sc.SetState(SCE_B_COMMENT_BLOCK);
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
				sc.Forward();
			} else if (sc.MatchAGKRem()) {
				sc.SetState(SCE_B_COMMENT);
				sc.Forward();
				sc.Forward();
				sc.Forward();
			} else if (sc.Match('"')) {
				sc.SetState(SCE_B_STRING);
			} else if (sc.Match('\'')) {
				sc.SetState(SCE_B_STRING2);
			} else if (sc.Match('0', 'x')) {
				sc.SetState(SCE_B_HEXNUMBER);
				sc.Forward();
			} else if (sc.Match('%')) {
				sc.SetState(SCE_B_BINNUMBER);
			} else if (IsDigit(sc.ch)) {
				sc.SetState(SCE_B_NUMBER);
			} else if (IsOperator(sc.ch)) {
				sc.SetState(SCE_B_OPERATOR);
			} else if (IsIdentifier(sc.ch)) {
				wasfirst = isfirst;
				sc.SetState(SCE_B_IDENTIFIER);
			} else if (!IsSpace(sc.ch)) {
				sc.SetState(SCE_B_ERROR);
			}
		}

		if (!IsSpace(sc.ch))
			isfirst = false;

		if (!sc.More())
			break;
	}
	sc.Complete();
}


void SCI_METHOD LexerBasic::Fold(unsigned int startPos, int length, int /* initStyle */, IDocument *pAccess) {

	if (!options.fold)
		return;

	LexAccessor styler(pAccess);

	int line = styler.GetLine(startPos);
	int level = styler.LevelAt(line);
	int go = 0, done = 0;
	int endPos = startPos + length;
	char word[256];
	int wordlen = 0;
	const bool userDefinedFoldMarkers = !options.foldExplicitStart.empty() && !options.foldExplicitEnd.empty();
	int cNext = styler[startPos];

	// Scan for tokens at the start of the line (they may include
	// whitespace, for tokens like "End Function"
	for (int i = startPos; i < endPos; i++) {
		int c = cNext;
		cNext = styler.SafeGetCharAt(i + 1);
		bool atEOL = (c == '\r' && cNext != '\n') || (c == '\n');
		if (options.foldSyntaxBased && !done && !go ) {
			if (wordlen) { // are we scanning a token already?
				word[wordlen] = static_cast<char>(LowerCase(c));
				if (!IsIdentifier(c) && c != '/' && c != '*') { // done with token
					word[wordlen] = '\0';
					if ( styler.StyleAt(i-1) != SCE_B_COMMENT_BLOCK ) go = CheckFoldPoint(word, level);
					else 
					{
						if ( !strcmp(word, "remstart") || !strcmp(word, "/*") )
						{
							level |= SC_FOLDLEVELHEADERFLAG;
							go = 1;
						}
						else if ( !strcmp(word, "remend") || !strcmp(word, "*/") )
						{
							go = -1;
						}
						else go = 0;
					}

					if (!go) {
						// Treat any whitespace as single blank, for
						// things like "End   Function".
						if (IsSpace(c) && IsIdentifier(word[wordlen - 1])) {
							word[wordlen] = ' ';
							if (wordlen < 255)
								wordlen++;
						}
						else // done with this line
							done = 1;
					}
				} else if (wordlen < 255) {
					wordlen++;
				}
			} else { // start scanning at first non-whitespace character
				if (!IsSpace(c)) {
					if (IsIdentifier(c) || c == '/' || c == '*') {
						word[0] = static_cast<char>(LowerCase(c));
						wordlen = 1;
					} else // done with this line
						done = 1;
				}
			}
		}
		/*
		if (options.foldCommentExplicit && ((styler.StyleAt(i) == SCE_B_COMMENT) || options.foldExplicitAnywhere)) {
			if (userDefinedFoldMarkers) {
				if (styler.Match(i, options.foldExplicitStart.c_str())) {
 					level |= SC_FOLDLEVELHEADERFLAG;
					go = 1;
				} else if (styler.Match(i, options.foldExplicitEnd.c_str())) {
 					go = -1;
 				}
			} else {
				if (c == comment_char) {
					if (cNext == '{') {
						level |= SC_FOLDLEVELHEADERFLAG;
						go = 1;
					} else if (cNext == '}') {
						go = -1;
					}
				}
 			}
 		}
		*/
		if (atEOL) { // line end
			if (!done && wordlen == 0 && options.foldCompact) // line was only space
				level |= SC_FOLDLEVELWHITEFLAG;
			if (level != styler.LevelAt(line))
				styler.SetLevel(line, level);
			level += go;
			line++;
			// reset state
			wordlen = 0;
			level &= ~SC_FOLDLEVELHEADERFLAG;
			level &= ~SC_FOLDLEVELWHITEFLAG;
			go = 0;
			done = 0;
		}
	}
}

LexerModule lmAGKBasic(SCLEX_AGKBASIC, LexerBasic::LexerFactoryAGKBasic, "AGK", agkWordListDesc);
