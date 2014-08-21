/* Copyright 1996-2012 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 Resource layer for Seismic Duck
*******************************************************************************/

#pragma once
#ifndef BuiltFromResource_H
#define BuiltFromResource_H

#include "StartupList.h"

class NimblePixMap;
class HostResourceLoader;

class BuiltFromResource: public StartupListItem<BuiltFromResource> {
    const char* const myResourceName;
    virtual void hostLoad() = 0;
protected:
    BuiltFromResource( const char* resourceName_ ) : myResourceName(resourceName_) {}
public:
    const char* resourceName() const {return myResourceName;}

    //! Load all resources.
    static void loadAll();
};

class BuiltFromResourcePixMap: public BuiltFromResource {
    /*override*/ void hostLoad();
public:
    BuiltFromResourcePixMap( const char* resourceName_ ) : BuiltFromResource(resourceName_) {}

    virtual void buildFrom( const NimblePixMap& map ) = 0;
};

class BuiltFromResourceWaveform: public BuiltFromResource {
    /*override*/ void hostLoad();
public:
    BuiltFromResourceWaveform( const char* resourceName_ ) : BuiltFromResource(resourceName_) {}

    virtual void buildFrom( const char* data, size_t size ) = 0;
};

#endif /* BuiltFromResource_H */