#include <string>
#include "Orchestra.h"

void ReadSF2( const std::string& path );
void ReadFreePatConfig( const std::string& path );
Synthesizer::SoundSet* GetDefaultSoundSet( unsigned midiNumber );