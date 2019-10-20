# quantified-typing

Measuring typing speed over the day.

* Author: `Felix Kaiser <felix.kaiser@fxkr.net>`
* SPDX license code: GPL-3.0-or-later
* Status: alpha - using it myself (Oct 2019)

This tool collects the delays between key presses in a journal.

The journal consists of lines, each representing a 5 minute interval.
Each line consists of a timestamp, as well as buckets.
Each bucket represents a delay between two keypresses (by default, the buckets are 0sec to 2sec, with 10msec spacing, and an overflow bucket).
The bucket value is how often that delay between two key presses occured with the interval.

A journal line might look like this:

```
{"t":"1571549400","l":"2019-10-19 22:30:00","e":{"0":7,"10":11,"20":17,"30":57,"40":56,"50":57,"60":72,"70":102,"80":95,"90":51,"100":53,"110":49,"120":61,"130":46,"140":59,"150":60,"160":37,"170":28,"180":17,"190":37,"200":22,"210":7,"220":7,"230":12,"240":12,"250":10,"260":12,"270":16,"280":13,"290":7,"300":5,"310":4,"320":9,"330":6,"340":1,"350":3,"360":4,"370":2,"380":8,"390":4,"400":4,"410":4,"420":3,"430":4,"440":4,"450":1,"460":3,"470":1,"490":1,"510":3,"520":1,"530":1,"540":1,"550":2,"560":3,"570":2,"580":3,"590":1,"600":1,"630":4,"640":1,"660":1,"670":1,"680":1,"710":1,"720":2,"730":1,"750":1,"770":1,"810":1,"820":1,"830":2,"880":2,"920":1,"930":1,"1010":1,"1070":1,"1110":2,"1120":1,"1140":1,"1160":1,"1200":1,"1290":1,"1320":1,"1610":1,"1680":1,"1710":1,"1750":1,"1790":1,"1820":2,"1910":1,"1930":1,"1990":1,"inf":33}}
```
