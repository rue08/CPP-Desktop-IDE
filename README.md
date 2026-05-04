# C++ Desktop IDE

## Overview
A lightweight and self-contained **C++ IDE**, designed to solve the problem of accessing and managing code across multiple machines.


Cloud backup systems have made personal data such as files and photos easily accessible from anywhere, enabling seamless creation, modification, and synchronisation. However, this level of convenience is not commonly available for raw source code in a structured development environment.


This project solves addresses this gap by making the raw code files available anywhere and at any time with just a simple Sign_Up, giving the user freedom to manage their code from any machine.

## Features
- Designed a custom C++ IDE environment using Qt.
- Integrated a process manager to drive the ```g++``` toolchain for compilation and execution.
- Built a cloud-backed file system using REST APIs via QtNetwork.
- Supports asynchronous upload/download of code files.
- Implemented real-time file management and error handling.

## Tech Stack
- **Language**: C++
- **Framework**: Qt
- **BaaS + Cloud**: Firebase
- **Compiler**: g++
- **Networking**: QtNetwork (REST APIs)

## Installation
### Prerequisites
- Qt Creator
- g++/Clang

### Steps
1. Open Terminal.
2. Change the current working directory to the location where you want the cloned directory.
3. Type ```git clone https://github.com/rue08/CPP-Desktop-IDE.git``` and hit Enter.
4. Open the Qt Creator and from the File in the menubar, click Open Project, navigate to the cloned folder.
5. Click on ```CmakeLists.txt``` file and hit Enter.
6. In the bottom left corner, hit Run.

## Project Status
The main aim is to build a full featured project and therefore it is under development.

### Some feature still to come
1. Move from Firebase to Node.js backend.
2. More robust file handling with an option to upload folders. Currently only code files can be uploaded.
3. Improved UI/UX.

## Author
Mehul Sharma\
Gmail: mehssi2004@gmail.com

If you have any suggestions feel free to reach out to me via mail and if you liked the project, make sure to give a star ⭐️.
