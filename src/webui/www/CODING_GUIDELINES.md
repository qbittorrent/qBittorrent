# Coding Guidelines

All new code MUST follow the following guidelines. \
Most of the rules are already enforced by static analyzers invoked via `npm`. Additional rules are as follows.

## Do Not Use MooTools Library

The [MooTools library][mootools-home] has seen [little maintenance][mootools-code-frequency] and therefore,
its usage is [prohibited][mootools-deprecate]. \
New code should use vanilla JavaScript and resort to MooTools only if strictly necessary. \
See also: [CVE-2021-20088](https://www.cve.org/CVERecord?id=CVE-2021-20088)

[mootools-home]: https://mootools.net/
[mootools-code-frequency]: https://github.com/mootools/mootools-core/graphs/code-frequency
[mootools-deprecate]: https://github.com/mootools/mootools-core/issues/2798

## Do Not Modify Translation Files

You must not modify any files in the `translations` folder. \
This task is handled by the project maintainer, so no action is required on your part.

## Do Not Use Implicit Conversion to Boolean

```js
const element = document.getElementById("searchPattern");

// Bad
if (!element)
    return;

// Good
if (element === null)
    return;
```

## Not Covered Above

If something isn't covered above, follow the existing style of the file you are editing. \
*This guide is not exhaustive; unmentioned rules may be determined by project members during code review.*
