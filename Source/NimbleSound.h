/* Copyright 1996-2015 Arch D. Robison 

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
 Sound layer
*******************************************************************************/

//! Default sample rate
const size_t NimbleSoundSamplesPerSec = 44100;

//! Floating-point sound sample.
/** Amplitude should be between -1 and 1. */
typedef float NimbleSoundSample;

typedef void (*OutputInterruptHandlerType)( float* left, float* right, unsigned n );

void SetOutputInterruptHandler(OutputInterruptHandlerType handler);