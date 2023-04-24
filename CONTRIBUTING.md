# Contributing Guidelines
We believe that *memdbgvis* has great potential for expansion and improvement, and we would be grateful for any contributions from the community through open-source collaboration. This document serves as our official guide for contributing to this project. We value and appreciate all contributions, and we believe that working together with our community will lead to the best possible outcome. Before contributing, we kindly ask that you review these guidelines to ensure that your contributions align with our standards and expectations.

## Areas of Focus
We welcome contributions of all types, including feature additions and other enhancements. However, to maintain the integrity and nature of our project, there are certain elements that we seek to preserve. In addition, please note that we prioritize contributions based on their alignment with our project's goals and objectives. Thank you for considering these factors when contributing to our project. When contributing, please refer to the following tasks in numerical order, with the most important tasks listed first:
1. If you encounter any fatal bugs that result in *memdbgvis* or the JVM crashing, it is imperative that you promptly report the problem by creating an issue.  Furthermore, if you have an immediate fix for a bug, we kindly request that you create an issue first to keep a record of the problem, and then submit your fix as a pull request, making sure to reference the original issue. This will help us to better track and manage the issue, while ensuring that the fix is properly reviewed and tested before being incorporated into the project.
2. If you encounter non-fatal bugs that do not cause the program to crash but still affect its intended functionality, we request that you report these issues in a manner similar to fatal bugs. If you have a fix to the bug, follow the steps above to create a pull request linked to the original issue.
3. Bugs that do not alter or change the features of the program, such as typographical errors, grammatical mistakes, or ambiguous documentation, are considered low priority issues. Although these issues may still need to be addressed at some point, they should only be tackled after higher priority issues have been resolved.

The priority list provided above is not exhaustive, and we encourage users to report any issues they encounter, regardless of whether they are explicitly listed or not. We welcome all bug reports and will evaluate each issue on a case-by-case basis to determine its priority and whether it should be addressed.

## Feature Additions
In addition to our primary goal of fixing any existing issues, we also welcome contributions to add new features to our project. Before doing so, please review the following guidelines to ensure that the proposed features align with our goals:
- Currently, *memdbgvis* is designed and tested for Windows 10+ systems only. We would welcome is the ability to make the visualizer portable to other operating systems, such as Linux.
- You can add helpful insights, such as tips and tricks, to assist new users in getting the most out of *memdbgvis*.
- Another potential new feature is the addition of support for higher-dimensional arrays, which was not implemented in the Heap Inspector due to resource and time constraints. If you are interested in contributing to this effort, you could help us to add this functionality and become a valuable contributor to the project.

It is important to note that some feature additions may fundamentally change the identity of *memdbgvis*. Therefore, we kindly ask contributors to refrain from proposing or adding features that may significantly deviate from the project's goals. Examples of features that may not align with the project's goals:
- The implementation of a mutable, dynamic debugger that allows for modification of the memory during runtime.
- Adding support for debugging a programming language other than Java.

It is important to note that while such features may not be appropriate for the official project, it does not mean you cannot create derivatives of *memdbgvis* and add these features in. Our license allows for this, but please read it carefully before proceeding. Finally, any feature additions must be submitted in the form of a well-documented pull request.

## Configuring the Project for Local Development
To begin developing *memdbgvis* in your local environment, it is recommended to fork and clone the repository. In order to access the solution, Visual Studio 2022 is required. Following this, you are free to choose the following options:

- If you want to contribute to the JVM agent, open the C++ project titled "jvmagent."
    1. To build this project, it is essential to have the latest JDK downloaded and properly installed. Additionally, it is crucial to configure your linker to point to the appropriate libraries. In order to resolve `jvmti.h`, you can refer to online tutorials to set up your environment accordingly.
    2. Once you have made any necessary changes to the code, right-click on the project and select "Rebuild" in order to generate a new DLL that incorporates your updated changes.
- If you want to contribute to the visualizer interface, open the C++ project titled "visualizer."
    1. To build this project, it's necessary to install the latest version of the Qt framework. You can download it from the following link: https://download.qt.io/official_releases/online_installers/. In addition, you will need to install the Qt Visual Studio Tools extension in order for the project to build.
    2. Once you have made any necessary changes to the code, run the solution in "Release" mode. To deploy your release, take all of the required binaries and compress it into a zip archive. This step is not very strict since feature changes may require alterations to the release package.

No matter which choice you choose to work on, these final steps are essential for every local developer:

1. Commit and push your changes to your forked repository of *memdbgvis*.
2. Create a pull request on the official memdbgvis repository page, specifying the changes you have made.
3. After your changes are reviewed, we may ask you to make some additional edits or formatting adjustments. Once everything looks good, we will merge your modifications onto the official branch. Congratulations, you have successfully contributed to memdbgvis!