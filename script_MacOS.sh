clang++ src/*.cpp -o RLPlaneSim \
  -std=c++17 \
  -DPLATFORM_DESKTOP \
  -lraylib \
  -framework CoreVideo \
  -framework IOKit \
  -framework Cocoa \
  -framework GLUT \
  -framework OpenGL

#open RLPlaneSim

#works for compiling the game on mac (need to have raylib installed via homebrew (actually no longer))
#run the file `RLPlaneSim`` on mac

em++ -o game.html src/main.cpp -Os -Wall ./raylibsrc/libraylib.a -I ./raylibsrc -L ./raylibsrc  -s USE_GLFW=3 -s TOTAL_MEMORY=134217728 -s STACK_SIZE=5MB --preload-file ./src/assets --shell-file ./raylibsrc/minshell.html -DPLATFORM_WEB

#written according to https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5)

python3 -m http.server 3000