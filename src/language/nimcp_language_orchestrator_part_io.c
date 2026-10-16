// nimcp_language_orchestrator_part_io.c - io functions
// Part of nimcp_language_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_language_orchestrator.c


//=============================================================================
// String Conversion - from nimcp_language_types.h
//=============================================================================

const char* language_state_to_string(language_state_t state)
{
    switch (state) {
        case LANGUAGE_STATE_IDLE:          return "IDLE";
        case LANGUAGE_STATE_LISTENING:     return "LISTENING";
        case LANGUAGE_STATE_COMPREHENDING: return "COMPREHENDING";
        case LANGUAGE_STATE_INTEGRATING:   return "INTEGRATING";
        case LANGUAGE_STATE_GENERATING:    return "GENERATING";
        case LANGUAGE_STATE_PRODUCING:     return "PRODUCING";
        case LANGUAGE_STATE_ERROR:         return "ERROR";
        default:                           return "UNKNOWN";
    }
}


const char* language_mode_to_string(language_mode_t mode)
{
    switch (mode) {
        case LANGUAGE_MODE_COMPREHENSION: return "COMPREHENSION";
        case LANGUAGE_MODE_PRODUCTION:    return "PRODUCTION";
        case LANGUAGE_MODE_DIALOGUE:      return "DIALOGUE";
        case LANGUAGE_MODE_REPETITION:    return "REPETITION";
        case LANGUAGE_MODE_TRANSLATION:   return "TRANSLATION";
        default:                          return "UNKNOWN";
    }
}


const char* language_input_type_to_string(language_input_type_t type)
{
    switch (type) {
        case LANGUAGE_INPUT_AUDIO:    return "AUDIO";
        case LANGUAGE_INPUT_TEXT:     return "TEXT";
        case LANGUAGE_INPUT_TOKENS:   return "TOKENS";
        case LANGUAGE_INPUT_PHONEMES: return "PHONEMES";
        case LANGUAGE_INPUT_SEMANTIC: return "SEMANTIC";
        case LANGUAGE_INPUT_VISUAL:   return "VISUAL";
        default:                      return "UNKNOWN";
    }
}


const char* language_output_type_to_string(language_output_type_t type)
{
    switch (type) {
        case LANGUAGE_OUTPUT_MOTOR_COMMANDS: return "MOTOR_COMMANDS";
        case LANGUAGE_OUTPUT_PHONEMES:       return "PHONEMES";
        case LANGUAGE_OUTPUT_TOKENS:         return "TOKENS";
        case LANGUAGE_OUTPUT_TEXT:           return "TEXT";
        case LANGUAGE_OUTPUT_SEMANTIC:       return "SEMANTIC";
        default:                             return "UNKNOWN";
    }
}


const char* phoneme_category_to_string(phoneme_category_t cat)
{
    switch (cat) {
        case PHONEME_CAT_VOWEL:       return "VOWEL";
        case PHONEME_CAT_STOP:        return "STOP";
        case PHONEME_CAT_FRICATIVE:   return "FRICATIVE";
        case PHONEME_CAT_AFFRICATE:   return "AFFRICATE";
        case PHONEME_CAT_NASAL:       return "NASAL";
        case PHONEME_CAT_APPROXIMANT: return "APPROXIMANT";
        case PHONEME_CAT_SILENCE:     return "SILENCE";
        case PHONEME_CAT_UNKNOWN:     return "UNKNOWN";
        default:                      return "UNKNOWN";
    }
}


const char* part_of_speech_to_string(part_of_speech_t pos)
{
    switch (pos) {
        case POS_NOUN:          return "NOUN";
        case POS_VERB:          return "VERB";
        case POS_ADJECTIVE:     return "ADJECTIVE";
        case POS_ADVERB:        return "ADVERB";
        case POS_DETERMINER:    return "DETERMINER";
        case POS_PREPOSITION:   return "PREPOSITION";
        case POS_CONJUNCTION:   return "CONJUNCTION";
        case POS_PRONOUN:       return "PRONOUN";
        case POS_AUXILIARY:     return "AUXILIARY";
        case POS_COMPLEMENTIZER:return "COMPLEMENTIZER";
        case POS_NEGATION:      return "NEGATION";
        case POS_PUNCTUATION:   return "PUNCTUATION";
        case POS_UNKNOWN:       return "UNKNOWN";
        default:                return "UNKNOWN";
    }
}


const char* thematic_role_to_string(thematic_role_t role)
{
    switch (role) {
        case THEMATIC_ROLE_AGENT:       return "AGENT";
        case THEMATIC_ROLE_PATIENT:     return "PATIENT";
        case THEMATIC_ROLE_THEME:       return "THEME";
        case THEMATIC_ROLE_EXPERIENCER: return "EXPERIENCER";
        case THEMATIC_ROLE_BENEFICIARY: return "BENEFICIARY";
        case THEMATIC_ROLE_INSTRUMENT:  return "INSTRUMENT";
        case THEMATIC_ROLE_LOCATION:    return "LOCATION";
        case THEMATIC_ROLE_SOURCE:      return "SOURCE";
        case THEMATIC_ROLE_GOAL:        return "GOAL";
        case THEMATIC_ROLE_TIME:        return "TIME";
        case THEMATIC_ROLE_MANNER:      return "MANNER";
        case THEMATIC_ROLE_CAUSE:       return "CAUSE";
        case THEMATIC_ROLE_NONE:        return "NONE";
        default:                        return "UNKNOWN";
    }
}


const char* phrase_type_to_string(phrase_type_t type)
{
    switch (type) {
        case PHRASE_NP:   return "NP";
        case PHRASE_VP:   return "VP";
        case PHRASE_PP:   return "PP";
        case PHRASE_AP:   return "AP";
        case PHRASE_ADVP: return "ADVP";
        case PHRASE_S:    return "S";
        case PHRASE_SBAR: return "SBAR";
        case PHRASE_CP:   return "CP";
        case PHRASE_IP:   return "IP";
        case PHRASE_DP:   return "DP";
        default:          return "UNKNOWN";
    }
}


const char* parse_state_to_string(parse_state_t state)
{
    switch (state) {
        case PARSE_STATE_INIT:        return "INIT";
        case PARSE_STATE_ACTIVE:      return "ACTIVE";
        case PARSE_STATE_COMPLETE:    return "COMPLETE";
        case PARSE_STATE_AMBIGUOUS:   return "AMBIGUOUS";
        case PARSE_STATE_GARDEN_PATH: return "GARDEN_PATH";
        case PARSE_STATE_ERROR:       return "ERROR";
        default:                      return "UNKNOWN";
    }
}


const char* language_event_type_to_string(language_event_type_t type)
{
    switch (type) {
        case LANGUAGE_EVENT_UTTERANCE_START:       return "UTTERANCE_START";
        case LANGUAGE_EVENT_PHONEME_RECOGNIZED:    return "PHONEME_RECOGNIZED";
        case LANGUAGE_EVENT_WORD_RECOGNIZED:       return "WORD_RECOGNIZED";
        case LANGUAGE_EVENT_CONCEPT_ACTIVATED:     return "CONCEPT_ACTIVATED";
        case LANGUAGE_EVENT_COMPREHENSION_COMPLETE:return "COMPREHENSION_COMPLETE";
        case LANGUAGE_EVENT_PRODUCTION_START:      return "PRODUCTION_START";
        case LANGUAGE_EVENT_PRODUCTION_COMPLETE:   return "PRODUCTION_COMPLETE";
        case LANGUAGE_EVENT_AMBIGUITY_DETECTED:    return "AMBIGUITY_DETECTED";
        case LANGUAGE_EVENT_ANOMALY_DETECTED:      return "ANOMALY_DETECTED";
        case LANGUAGE_EVENT_ERROR:                 return "ERROR";
        case LANGUAGE_EVENT_STATE_CHANGE:          return "STATE_CHANGE";
        case LANGUAGE_EVENT_TRAINING_UPDATE:       return "TRAINING_UPDATE";
        default:                                   return "UNKNOWN";
    }
}
