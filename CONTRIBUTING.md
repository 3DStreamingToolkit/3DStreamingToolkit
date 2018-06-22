# Contribution Guidelines

Thanks for taking the time to read the contribution guidelines. We look forward to your contributions to the 3DStreamingToolkit project.

## Quick Links

- [Code of Conduct](#code-of-conduct)
- [Help Wanted](#help-wanted)
- [Filing Issues](#filing-issues)
- [Pull Requests](#pull-requests)
    - [General Guidelines](#general-guidelines)
    - [Testing Guidelines](#testing-guidelines)
    - [Coding Style](#coding-style)
    - [Copyright Headers](#copyright-headers)
    - [Contributor License Agreement](#contributor-license-agreement-cla)

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Help Wanted

We have a [list of issues](https://github.com/CatalystCode/3DStreamingToolkit/labels/help%20wanted) that are ripe for community contributions. This is a great place to get started.

## Filing Issues

You can find all of the issues that have been filed in the [Issues](https://github.com/CatalystCode/3DStreamingToolkit/issues) section of the repository.

If you encounter any bugs, please file an issue [here](https://github.com/CatalystCode/3DStreamingToolkit/issues/new) and make sure to fill out the provided template with the requested information.

To suggest a new feature or changes that could be made, file an issue the same way you would for a bug, but remove the provided template and replace it with information about your suggestion.

## Pull Requests

If you are thinking about making a large change to this library, **break up the change into small, logical, testable chunks, and organize your pull requests accordingly**.

You can find all of the pull requests that have been opened in the [Pull Request](https://github.com/CatalystCode/3DStreamingToolkit/pulls) section of the repository.

To open your own pull request, click [here](https://github.comCatalystCode/3DStreamingToolkit/compare). When creating a pull request, keep the following in mind:
- Make sure you are pointing to the fork and branch that your changes were made in
- The pull request template that is provided **should be filled out**; this is not something that should just be deleted or ignored when the pull request is created
    - Deleting or ignoring this template will elongate the time it takes for your pull request to be reviewed

### General Guidelines

The following guidelines must be followed in **EVERY** pull request that is opened.

- Title of the pull request is clear and informative
- There are a small number of commits that each have an informative message
- Pull request includes test coverage for the included changes

### Testing Guidelines

In addition to adding test coverage for the included changes, please ensure the following:

- Run the unit tests in the Visual Studio 2017 Test Explorer (see [below](#running-tests))
- Run a sample client and server locally

#### Running Tests

> We use [googltest](https://github.com/google/googletest) as a testing framework.

To run the tests in Visual Studio Test Explorer please follow these steps:

- Ensure you have installed the [Google Test Adapter](https://github.com/csoltenborn/GoogleTestAdapter) from [here](https://marketplace.visualstudio.com/items?itemName=ChristianSoltenborn.GoogleTestAdapter)
- Build the solution (Build -> Build Solution)
- Open the Visual Studio Test Explorer (Test -> Windows -> Test Explorer)
- Press "Run All" or select the tests you wish to run and press "Run"

To run the tests in the commandline:

Simply run `test` or `.\test.cmd` from the project directory:

```
C:\git\3dtoolkit>test
<test output>
```

### Coding Style

Refer to the [WebRTC coding style guide](https://webrtc.googlesource.com/src/+/HEAD/style-guide.md).

### Copyright Headers

Copy and paste the following copyright header to any new source code files:
```
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
```

### Contributor License Agreement (CLA)

In order to accept your pull request, we need you to submit a CLA. You only need to do this once for this repository. If you are submitting a pull request for the first time, the Microsoft CLA Bot will reply with a link to the CLA form. You may also complete your CLA [here](https://cla.opensource.microsoft.com/CatalystCode/3dtoolkit).
