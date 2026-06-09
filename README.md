<h1 align="center" style="border-bottom: none;">Pet for desktop 🦊</h1>
<h3 align="center">A fox companion like you've never dreamed</h3>
<p align="center">
  <a href="LICENSE">
    <img alt="License" src="https://img.shields.io/badge/License-MIT-blue.svg">
  </a>
  <a href="https://github.com/Renardjojo/PetForDesktop/releases/latest">
    <img alt="Version" src="https://img.shields.io/github/release/Renardjojo/PetForDesktop">
  </a>
  <a href="#LastActivity">
    <img alt="LastActivity" src="https://img.shields.io/github/last-commit/Renardjojo/PetForDesktop">
  </a>
</p>
<p align="center">
  <a href="https://discord.gg/gjdQmHAp7e">
    <img alt="Join us on discord" height="26" width="91" src="https://img.shields.io/badge/join us-blue?logo=discord&logoColor=white">
  </a>
  <a href="https://www.patreon.com/PetForDesktop">
    <img alt="Join us on patreon" height="26" width="91" src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white">
  </a>
</p>


 
![image](https://user-images.githubusercontent.com/55276408/195999573-1e5f854b-230b-4e17-9920-6493975ed145.png)
![PetForDesktopDemonstration](https://user-images.githubusercontent.com/55276408/222144931-3546cc40-3989-4a36-8e5c-bf43e239ee2a.gif)
 
Welcome to PetForDesktop land!  
PetForDesktop allows you to integrate and interact with your pet and customize it whenever you want!  

## How to install?
Just download the [latest version](https://github.com/Renardjojo/PetDesktop/releases/latest) of this application and extract it.
On Windows, releases now also include a ready-to-run `windows-installer.exe` alongside the archive download.

## How to build?
The repo now ships modern CMake presets and an install-first packaging flow.

```bash
cmake --preset x64-Release_ogl
cmake --build --preset x64-Release_ogl
ctest --preset x64-Release_ogl
cmake --install out/x64-Release_ogl --prefix out/install/windows
cpack --config out/x64-Release_ogl/CPackConfig.cmake -G ZIP
```

If you prefer dependency fetching instead of the bundled submodules, enable `PETFORDESKTOP_USE_FETCHCONTENT_DEPS=ON` or point CMake at a vcpkg manifest/toolchain. The repo also keeps `installer/petForDesktop.iss` as a legacy fallback installer while the Windows release chain moves toward signed package formats.

## What you can do with PetForDesktop:
- Edge detection of screen content: your pet can jump into any element of your screen.
- Basic physic: you can drag and drop your pet with gravity but also make it bounce on your screen.
- Customize settings : to play with the debugging function, configure your own gravity etc...
- Customize animation
- Replace the existing animation with your own horizontal sprite sheet.
- Walk and jump but also edit and create your own animation!
- Modify the code and contribute! (it's open source and under MIT license!)
- Use multiple monitors
- Open a context menu to interact with it
- [Support development](https://www.patreon.com/PetForDesktop): Access the special discord channel, test new versions before anyone else and vote for the content of the next update.

## What is currently not possible with PetForDesktop:
- Cook and make your own coffee...
- Launch multiple pets (you can but it is not optimized)
- Works on macOS and Linux...

## What in the future?
You can easily check out the [roadmap](https://github.com/Renardjojo/PetForDesktop/milestones) but remember that this software is open source.  
That means you can fork it and modify or contribute to it for anyone! The MTI license allows you to do this, so take advantage of it.  

## Disclamer:
PetForDesktop also takes small screenshots of your screen to process collision and edge detection. This application will **NEVER** save or send them! 

## Contributor
- *Hempuli* with its "[Babafriend](https://hempuli.itch.io/baba-friend)" application. The first inspiration for this concept was initiated through his program.  
- *LeGitHubDeTai* with his tool "[github-to-discord](https://github.com/LeGitHubDeTai/github-to-discord)" to generate a notification on discord when new versions are released.  
- *elthen* with his [fox sprite sheet](https://elthen.itch.io/2d-pixel-art-fox-sprites) which I used and modified to suit my needs.  
- *cassala* with his [bubble sprite sheet](https://cassala.itch.io/bubble-sprites) which I used and modified to suit my needs.  
- *jbeder and all his contributors* thanks to [yaml-cpp](https://github.com/jbeder/yaml-cpp) I was able to save parameters in a very readable format.
- *aaronmjacobs and all his contributors* thanks to [Boxer](https://github.com/aaronmjacobs/Boxer) I've been able to open error, warning and information pop-ups if something goes wrong with the application cross-platform.
- Thanks to all the contributors to [cpr](https://github.com/libcpr/cpr), I've been able to update and download it.
- Thanks to all the contributors to [glfw](https://github.com/glfw/glfw) I was able to create a window and interact with it in a cross-plaftorm way.
- *ocornuta and all it's contributor* thanks to its wonderful library [Dear ImGui](https://github.com/ocornut/imgui) I was able to add a custom user interface very easily.
