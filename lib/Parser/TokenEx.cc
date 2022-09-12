#include "TokenEx.h"

namespace scatha {

	using namespace parse;
	
	TokenEx expand(Token const& token) {
		TokenEx result{ token };
		
		if (token.type == TokenType::Punctuation) {
			result.isPunctuation = true;
			if (token.id == ";" || token.id == "EOL") {
				result.isSeparator = true;
			}
		}
		
		if (std::optional<Keyword> const keyword = toKeyword(token)) {
			result.isKeyword = true;
			result.keyword = *keyword;
			result.keywordCategory = categorize(*keyword);
		}
		
		if (token.type == TokenType::Identifier) {
			result.isIdentifier = true;
		}
		
		return result;
	}
	
}
