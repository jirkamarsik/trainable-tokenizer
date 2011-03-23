#include <cstdlib>
#include <string>
#include <boost/unordered_map.hpp>

#include "text_cleaner.hpp"
#include "trtok_clean_entities_EntityCleaner"
#include "trtok_clean_xml_XmlCleaner"
#include "cutout_t.hpp"
#include "utils.hpp"


/* Due to the nature of XML (contains whitespace) and Quex, one of two
 * preprocessors is used depending on whether the user wants to hide XML
 * or not. Since the preprocessors are defined using different types
 * in different namespaces, the code is repeated twice for both of them
 * through macros. */
#define TEXTCLEANER(cleaner_namespace, cleaner_class, token_prefix) cleaner_namespace::cleaner_class lex(m_input_stream_p, m_input_encoding);\
		lex.expand_entities = m_expand_entities;\
		cleaner_namespace::Token *token_p = 0x0;\
\
		do {\
			lex.receive(&token_p);\
\
			if (token_p->type_id() == token_prefix##TEXT) {\
				*m_output_stream_p << unicode_to_utf8(token_p->get_text());\
			}\
			else if (token_p->type_id() == token_prefix##ENTITY) {\
				if (m_expand_entities) {\
					cutout_t cutout;\
					cutout.kind = ENTITY;\
					cutout.position = token_p->number;\
					cutout.text = unicode_to_utf8(token_p->get_text());\
					\
					std::string entity = unicode_to_utf8(token_p->get_text());\
					std::string expanded;\
					bool good_expand = expand_entity(entity, expanded);\
					if (good_expand) {\
						if ((m_cutout_queue_p != 0x0) && !m_keep_entities_expanded) {\
							m_cutout_queue_p->push(cutout);\
							*m_output_stream_p << expanded;\
						} else {\
							*m_output_stream_p << expanded;\
						}\
					} else {\
							*m_output_stream_p << entity;\
					}\
				} else\
					*m_output_stream_p << unicode_to_utf8(token_p->get_text());\
			} else if (token_p->type_id() == token_prefix##XML) {\
				cutout_t cutout;\
				cutout.kind = XML;\
				cutout.position = token_p->number;\
				cutout.text = unicode_to_utf8(token_p->get_text());\
				if (m_cutout_queue_p != 0x0)\
					m_cutout_queue_p->push(cutout);\
			}\
		} while (token_p->type_id() != token_prefix##TERMINATION)

namespace trtok {

inline std::string unicode_to_utf8(std::basic_string<uint32_t> const &str)
{
	return clean_entities::EntityCleaner_unicode_to_char(str);
}

bool TextCleaner::expand_entity(std::string const &entity, std::string &expanded_str)
{
	uint32_t expanded_char = 0;
	bool success = false;

	if (entity[1] == '#') {
		char const *entity_p = entity.c_str();
		char *conv_out_p;
		if ((entity[2] == 'x') || (entity[2] == 'X'))
			expanded_char = strtoul(entity_p + 3, &conv_out_p, 16);
		else
			expanded_char = strtoul(entity_p + 2, &conv_out_p, 10);
		success = (conv_out_p - entity_p == entity.length() - 1)
				&& (expanded_char > 1);
				//0x0 and 0x1 are characters we do not want in the Quex stream
	} else {
		typedef boost::unordered_map<std::string, uint32_t>::const_iterator iter;
		iter lookup = m_entity_map.find(entity.substr(1, entity.length() - 2));
		if (lookup != m_entity_map.end()) {
			success = true;
			expanded_char = lookup->second;
		}
	}

	expanded_str = unicode_to_utf8(std::basic_string<uint32_t>(1, expanded_char));
	return success;
}

void TextCleaner::do_work()
{
	if (!m_hide_xml)
	{
		TEXTCLEANER(clean_entities, EntityCleaner, QUEX_PREPROC_NOXML_);
	}
	else
	{
		TEXTCLEANER(clean_xml, XmlCleaner, QUEX_PREPROC_WITHXML_);
	}
}

void TextCleaner::prepare_entity_map()
{
	m_entity_map = boost::unordered_map<std::string, uint32_t>(512);

	// Although entity names are case sensitive, these most common entities
	// can sometimes be encountered in CAPS, which e.g. modern browsers
	// expand as well.
	m_entity_map["quot"] = 34;
	m_entity_map["QUOT"] = 34;
	m_entity_map["apos"] = 39;
	m_entity_map["APOS"] = 39;
	m_entity_map["amp"] = 38;
	m_entity_map["AMP"] = 38;
	m_entity_map["lt"] = 60;
	m_entity_map["LT"] = 60;
	m_entity_map["gt"] = 62;
	m_entity_map["GT"] = 62;

	// Taken from http://www.w3schools.com/tags/ref_entities.asp and processed
	// with vim command :'<,'>s/^\s*.\s*&#\([0-9]\+\);\s*&\([0-9a-zA-Z]\+\);.*$/\tm_entity_map\["\2"\] = \1;/g
	m_entity_map["nbsp"] = 160;
	m_entity_map["iexcl"] = 161;
	m_entity_map["cent"] = 162;
	m_entity_map["pound"] = 163;
	m_entity_map["curren"] = 164;
	m_entity_map["yen"] = 165;
	m_entity_map["brvbar"] = 166;
	m_entity_map["sect"] = 167;
	m_entity_map["uml"] = 168;
	m_entity_map["copy"] = 169;
	m_entity_map["ordf"] = 170;
	m_entity_map["laquo"] = 171;
	m_entity_map["not"] = 172;
	m_entity_map["shy"] = 173;
	m_entity_map["reg"] = 174;
	m_entity_map["macr"] = 175;
	m_entity_map["deg"] = 176;
	m_entity_map["plusmn"] = 177;
	m_entity_map["sup2"] = 178;
	m_entity_map["sup3"] = 179;
	m_entity_map["acute"] = 180;
	m_entity_map["micro"] = 181;
	m_entity_map["para"] = 182;
	m_entity_map["middot"] = 183;
	m_entity_map["cedil"] = 184;
	m_entity_map["sup1"] = 185;
	m_entity_map["ordm"] = 186;
	m_entity_map["raquo"] = 187;
	m_entity_map["frac14"] = 188;
	m_entity_map["frac12"] = 189;
	m_entity_map["frac34"] = 190;
	m_entity_map["iquest"] = 191;
	m_entity_map["times"] = 215;
	m_entity_map["divide"] = 247;
	m_entity_map["Agrave"] = 192;
	m_entity_map["Aacute"] = 193;
	m_entity_map["Acirc"] = 194;
	m_entity_map["Atilde"] = 195;
	m_entity_map["Auml"] = 196;
	m_entity_map["Aring"] = 197;
	m_entity_map["AElig"] = 198;
	m_entity_map["Ccedil"] = 199;
	m_entity_map["Egrave"] = 200;
	m_entity_map["Eacute"] = 201;
	m_entity_map["Ecirc"] = 202;
	m_entity_map["Euml"] = 203;
	m_entity_map["Igrave"] = 204;
	m_entity_map["Iacute"] = 205;
	m_entity_map["Icirc"] = 206;
	m_entity_map["Iuml"] = 207;
	m_entity_map["ETH"] = 208;
	m_entity_map["Ntilde"] = 209;
	m_entity_map["Ograve"] = 210;
	m_entity_map["Oacute"] = 211;
	m_entity_map["Ocirc"] = 212;
	m_entity_map["Otilde"] = 213;
	m_entity_map["Ouml"] = 214;
	m_entity_map["Oslash"] = 216;
	m_entity_map["Ugrave"] = 217;
	m_entity_map["Uacute"] = 218;
	m_entity_map["Ucirc"] = 219;
	m_entity_map["Uuml"] = 220;
	m_entity_map["Yacute"] = 221;
	m_entity_map["THORN"] = 222;
	m_entity_map["szlig"] = 223;
	m_entity_map["agrave"] = 224;
	m_entity_map["aacute"] = 225;
	m_entity_map["acirc"] = 226;
	m_entity_map["atilde"] = 227;
	m_entity_map["auml"] = 228;
	m_entity_map["aring"] = 229;
	m_entity_map["aelig"] = 230;
	m_entity_map["ccedil"] = 231;
	m_entity_map["egrave"] = 232;
	m_entity_map["eacute"] = 233;
	m_entity_map["ecirc"] = 234;
	m_entity_map["euml"] = 235;
	m_entity_map["igrave"] = 236;
	m_entity_map["iacute"] = 237;
	m_entity_map["icirc"] = 238;
	m_entity_map["iuml"] = 239;
	m_entity_map["eth"] = 240;
	m_entity_map["ntilde"] = 241;
	m_entity_map["ograve"] = 242;
	m_entity_map["oacute"] = 243;
	m_entity_map["ocirc"] = 244;
	m_entity_map["otilde"] = 245;
	m_entity_map["ouml"] = 246;
	m_entity_map["oslash"] = 248;
	m_entity_map["ugrave"] = 249;
	m_entity_map["uacute"] = 250;
	m_entity_map["ucirc"] = 251;
	m_entity_map["uuml"] = 252;
	m_entity_map["yacute"] = 253;
	m_entity_map["thorn"] = 254;
	m_entity_map["yuml"] = 255;

	// And here are some other entities from http://www.w3schools.com/tags/ref_symbols.asp
	// processed the same way
	m_entity_map["forall"] = 8704;
	m_entity_map["part"] = 8706;
	m_entity_map["exist"] = 8707;
	m_entity_map["empty"] = 8709;
	m_entity_map["nabla"] = 8711;
	m_entity_map["isin"] = 8712;
	m_entity_map["notin"] = 8713;
	m_entity_map["ni"] = 8715;
	m_entity_map["prod"] = 8719;
	m_entity_map["sum"] = 8721;
	m_entity_map["minus"] = 8722;
	m_entity_map["lowast"] = 8727;
	m_entity_map["radic"] = 8730;
	m_entity_map["prop"] = 8733;
	m_entity_map["infin"] = 8734;
	m_entity_map["ang"] = 8736;
	m_entity_map["and"] = 8743;
	m_entity_map["or"] = 8744;
	m_entity_map["cap"] = 8745;
	m_entity_map["cup"] = 8746;
	m_entity_map["int"] = 8747;
	m_entity_map["there4"] = 8756;
	m_entity_map["sim"] = 8764;
	m_entity_map["cong"] = 8773;
	m_entity_map["asymp"] = 8776;
	m_entity_map["ne"] = 8800;
	m_entity_map["equiv"] = 8801;
	m_entity_map["le"] = 8804;
	m_entity_map["ge"] = 8805;
	m_entity_map["sub"] = 8834;
	m_entity_map["sup"] = 8835;
	m_entity_map["nsub"] = 8836;
	m_entity_map["sube"] = 8838;
	m_entity_map["supe"] = 8839;
	m_entity_map["oplus"] = 8853;
	m_entity_map["otimes"] = 8855;
	m_entity_map["perp"] = 8869;
	m_entity_map["sdot"] = 8901;

	m_entity_map["Alpha"] = 913;
	m_entity_map["Beta"] = 914;
	m_entity_map["Gamma"] = 915;
	m_entity_map["Delta"] = 916;
	m_entity_map["Epsilon"] = 917;
	m_entity_map["Zeta"] = 918;
	m_entity_map["Eta"] = 919;
	m_entity_map["Theta"] = 920;
	m_entity_map["Iota"] = 921;
	m_entity_map["Kappa"] = 922;
	m_entity_map["Lambda"] = 923;
	m_entity_map["Mu"] = 924;
	m_entity_map["Nu"] = 925;
	m_entity_map["Xi"] = 926;
	m_entity_map["Omicron"] = 927;
	m_entity_map["Pi"] = 928;
	m_entity_map["Rho"] = 929;
	m_entity_map["Sigma"] = 931;
	m_entity_map["Tau"] = 932;
	m_entity_map["Upsilon"] = 933;
	m_entity_map["Phi"] = 934;
	m_entity_map["Chi"] = 935;
	m_entity_map["Psi"] = 936;
	m_entity_map["Omega"] = 937;
	 	 	 	 
	m_entity_map["alpha"] = 945;
	m_entity_map["beta"] = 946;
	m_entity_map["gamma"] = 947;
	m_entity_map["delta"] = 948;
	m_entity_map["epsilon"] = 949;
	m_entity_map["zeta"] = 950;
	m_entity_map["eta"] = 951;
	m_entity_map["theta"] = 952;
	m_entity_map["iota"] = 953;
	m_entity_map["kappa"] = 954;
	m_entity_map["lambda"] = 955;
	m_entity_map["mu"] = 956;
	m_entity_map["nu"] = 957;
	m_entity_map["xi"] = 958;
	m_entity_map["omicron"] = 959;
	m_entity_map["pi"] = 960;
	m_entity_map["rho"] = 961;
	m_entity_map["sigmaf"] = 962;
	m_entity_map["sigma"] = 963;
	m_entity_map["tau"] = 964;
	m_entity_map["upsilon"] = 965;
	m_entity_map["phi"] = 966;
	m_entity_map["chi"] = 967;
	m_entity_map["psi"] = 968;
	m_entity_map["omega"] = 969;
				  	 	 	 
	m_entity_map["thetasym"] = 977;
	m_entity_map["upsih"] = 978;
	m_entity_map["piv"] = 982;

	m_entity_map["OElig"] = 338;
	m_entity_map["oelig"] = 339;
	m_entity_map["Scaron"] = 352;
	m_entity_map["scaron"] = 353;
	m_entity_map["Yuml"] = 376;
	m_entity_map["fnof"] = 402;
	m_entity_map["circ"] = 710;
	m_entity_map["tilde"] = 732;
	m_entity_map["ensp"] = 8194;
	m_entity_map["emsp"] = 8195;
	m_entity_map["thinsp"] = 8201;
	m_entity_map["zwnj"] = 8204;
	m_entity_map["zwj"] = 8205;
	m_entity_map["lrm"] = 8206;
	m_entity_map["rlm"] = 8207;
	m_entity_map["ndash"] = 8211;
	m_entity_map["mdash"] = 8212;
	m_entity_map["lsquo"] = 8216;
	m_entity_map["rsquo"] = 8217;
	m_entity_map["sbquo"] = 8218;
	m_entity_map["ldquo"] = 8220;
	m_entity_map["rdquo"] = 8221;
	m_entity_map["bdquo"] = 8222;
	m_entity_map["dagger"] = 8224;
	m_entity_map["Dagger"] = 8225;
	m_entity_map["bull"] = 8226;
	m_entity_map["hellip"] = 8230;
	m_entity_map["permil"] = 8240;
	m_entity_map["prime"] = 8242;
	m_entity_map["Prime"] = 8243;
	m_entity_map["lsaquo"] = 8249;
	m_entity_map["rsaquo"] = 8250;
	m_entity_map["oline"] = 8254;
	m_entity_map["euro"] = 8364;
	m_entity_map["trade"] = 8482;
	m_entity_map["larr"] = 8592;
	m_entity_map["uarr"] = 8593;
	m_entity_map["rarr"] = 8594;
	m_entity_map["darr"] = 8595;
	m_entity_map["harr"] = 8596;
	m_entity_map["crarr"] = 8629;
	m_entity_map["lceil"] = 8968;
	m_entity_map["rceil"] = 8969;
	m_entity_map["lfloor"] = 8970;
	m_entity_map["rfloor"] = 8971;
	m_entity_map["loz"] = 9674;
	m_entity_map["spades"] = 9824;
	m_entity_map["clubs"] = 9827;
	m_entity_map["hearts"] = 9829;
	m_entity_map["diams"] = 9830;
}

}
