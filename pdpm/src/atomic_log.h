#pragma once

// Write a string out to a file, then immediately close the file handle to write it to disk.
// The specified file will be created in the PhysicsFS write path.
void atomic_log(char* message, char* file);
