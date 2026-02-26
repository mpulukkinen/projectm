#ifndef loopback_hpp
#define loopback_hpp

class projectMSDL;

bool initLoopback();
void configureLoopback(projectMSDL* app);
bool processLoopbackFrame(projectMSDL* app);

#endif /* loopback_hpp */
