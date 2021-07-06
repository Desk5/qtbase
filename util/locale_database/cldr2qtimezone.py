#!/usr/bin/env python3
#############################################################################
##
## Copyright (C) 2020 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################
"""Parse CLDR data for QTimeZone use with MS-Windows

Script to parse the CLDR common/supplemental/windowsZones.xml file and
encode for use in QTimeZone.  See ``./cldr2qlocalexml.py`` for where
to get the CLDR data.  Pass its root directory as first parameter to
this script and the qtbase root directory as second parameter.  It
shall update qtbase's src/corelib/time/qtimezoneprivate_data_p.h ready
for use.
"""

import os
import datetime
import textwrap

from localetools import unicode2hex, wrap_list, Error, SourceFileEditor
from cldr import CldrAccess

### Data that may need updates in response to new entries in the CLDR file ###

# This script shall report the updates you need to make, if any arise.
# However, you may need to research the relevant zone's standard offset.

# List of currently known Windows IDs.
# If this script reports missing IDs, please add them here.
# Look up the offset using (google and) timeanddate.com.
# Not public so may safely be changed.  Please keep in alphabetic order by ID.
# ( Windows Id, Offset Seconds )
windowsIdList = (
    ('Afghanistan Standard Time',        16200),
    ('Alaskan Standard Time',           -32400),
    ('Aleutian Standard Time',          -36000),
    ('Altai Standard Time',              25200),
    ('Arab Standard Time',               10800),
    ('Arabian Standard Time',            14400),
    ('Arabic Standard Time',             10800),
    ('Argentina Standard Time',         -10800),
    ('Astrakhan Standard Time',          14400),
    ('Atlantic Standard Time',          -14400),
    ('AUS Central Standard Time',        34200),
    ('Aus Central W. Standard Time',     31500),
    ('AUS Eastern Standard Time',        36000),
    ('Azerbaijan Standard Time',         14400),
    ('Azores Standard Time',             -3600),
    ('Bahia Standard Time',             -10800),
    ('Bangladesh Standard Time',         21600),
    ('Belarus Standard Time',            10800),
    ('Bougainville Standard Time',       39600),
    ('Canada Central Standard Time',    -21600),
    ('Cape Verde Standard Time',         -3600),
    ('Caucasus Standard Time',           14400),
    ('Cen. Australia Standard Time',     34200),
    ('Central America Standard Time',   -21600),
    ('Central Asia Standard Time',       21600),
    ('Central Brazilian Standard Time', -14400),
    ('Central Europe Standard Time',      3600),
    ('Central European Standard Time',    3600),
    ('Central Pacific Standard Time',    39600),
    ('Central Standard Time (Mexico)',  -21600),
    ('Central Standard Time',           -21600),
    ('China Standard Time',              28800),
    ('Chatham Islands Standard Time',    45900),
    ('Cuba Standard Time',              -18000),
    ('Dateline Standard Time',          -43200),
    ('E. Africa Standard Time',          10800),
    ('E. Australia Standard Time',       36000),
    ('E. Europe Standard Time',           7200),
    ('E. South America Standard Time',  -10800),
    ('Easter Island Standard Time',     -21600),
    ('Eastern Standard Time',           -18000),
    ('Eastern Standard Time (Mexico)',  -18000),
    ('Egypt Standard Time',               7200),
    ('Ekaterinburg Standard Time',       18000),
    ('Fiji Standard Time',               43200),
    ('FLE Standard Time',                 7200),
    ('Georgian Standard Time',           14400),
    ('GMT Standard Time',                    0),
    ('Greenland Standard Time',         -10800),
    ('Greenwich Standard Time',              0),
    ('GTB Standard Time',                 7200),
    ('Haiti Standard Time',             -18000),
    ('Hawaiian Standard Time',          -36000),
    ('India Standard Time',              19800),
    ('Iran Standard Time',               12600),
    ('Israel Standard Time',              7200),
    ('Jordan Standard Time',              7200),
    ('Kaliningrad Standard Time',         7200),
    ('Korea Standard Time',              32400),
    ('Libya Standard Time',               7200),
    ('Line Islands Standard Time',       50400),
    ('Lord Howe Standard Time',          37800),
    ('Magadan Standard Time',            36000),
    ('Magallanes Standard Time',        -10800), # permanent DST
    ('Marquesas Standard Time',         -34200),
    ('Mauritius Standard Time',          14400),
    ('Middle East Standard Time',         7200),
    ('Montevideo Standard Time',        -10800),
    ('Morocco Standard Time',                0),
    ('Mountain Standard Time (Mexico)', -25200),
    ('Mountain Standard Time',          -25200),
    ('Myanmar Standard Time',            23400),
    ('N. Central Asia Standard Time',    21600),
    ('Namibia Standard Time',             3600),
    ('Nepal Standard Time',              20700),
    ('New Zealand Standard Time',        43200),
    ('Newfoundland Standard Time',      -12600),
    ('Norfolk Standard Time',            39600),
    ('North Asia East Standard Time',    28800),
    ('North Asia Standard Time',         25200),
    ('North Korea Standard Time',        30600),
    ('Omsk Standard Time',               21600),
    ('Pacific SA Standard Time',        -10800),
    ('Pacific Standard Time',           -28800),
    ('Pacific Standard Time (Mexico)',  -28800),
    ('Pakistan Standard Time',           18000),
    ('Paraguay Standard Time',          -14400),
    ('Qyzylorda Standard Time',          18000), # a.k.a. Kyzylorda, in Kazakhstan
    ('Romance Standard Time',             3600),
    ('Russia Time Zone 3',               14400),
    ('Russia Time Zone 10',              39600),
    ('Russia Time Zone 11',              43200),
    ('Russian Standard Time',            10800),
    ('SA Eastern Standard Time',        -10800),
    ('SA Pacific Standard Time',        -18000),
    ('SA Western Standard Time',        -14400),
    ('Saint Pierre Standard Time',      -10800), # New France
    ('Sakhalin Standard Time',           39600),
    ('Samoa Standard Time',              46800),
    ('Sao Tome Standard Time',               0),
    ('Saratov Standard Time',            14400),
    ('SE Asia Standard Time',            25200),
    ('Singapore Standard Time',          28800),
    ('South Africa Standard Time',        7200),
    ('Sri Lanka Standard Time',          19800),
    ('Sudan Standard Time',               7200), # unless they mean South Sudan, +03:00
    ('Syria Standard Time',               7200),
    ('Taipei Standard Time',             28800),
    ('Tasmania Standard Time',           36000),
    ('Tocantins Standard Time',         -10800),
    ('Tokyo Standard Time',              32400),
    ('Tomsk Standard Time',              25200),
    ('Tonga Standard Time',              46800),
    ('Transbaikal Standard Time',        32400), # Yakutsk
    ('Turkey Standard Time',              7200),
    ('Turks And Caicos Standard Time',  -14400),
    ('Ulaanbaatar Standard Time',        28800),
    ('US Eastern Standard Time',        -18000),
    ('US Mountain Standard Time',       -25200),
    ('UTC-11',                          -39600),
    ('UTC-09',                          -32400),
    ('UTC-08',                          -28800),
    ('UTC-02',                           -7200),
    ('UTC',                                  0),
    ('UTC+12',                           43200),
    ('UTC+13',                           46800),
    ('Venezuela Standard Time',         -16200),
    ('Vladivostok Standard Time',        36000),
    ('Volgograd Standard Time',          14400),
    ('W. Australia Standard Time',       28800),
    ('W. Central Africa Standard Time',   3600),
    ('W. Europe Standard Time',           3600),
    ('W. Mongolia Standard Time',        25200), # Hovd
    ('West Asia Standard Time',          18000),
    ('West Bank Standard Time',           7200),
    ('West Pacific Standard Time',       36000),
    ('Yakutsk Standard Time',            32400),
    ('Yukon Standard Time',             -25200), # Non-DST Mountain Standard Time since 2020-11-01
)

# List of standard UTC IDs to use.  Not public so may be safely changed.
# Do not remove IDs, as each entry is part of the API/behavior guarantee.
# ( UTC Id, Offset Seconds )
utcIdList = (
    ('UTC',            0),  # Goes first so is default
    ('UTC-14:00', -50400),
    ('UTC-13:00', -46800),
    ('UTC-12:00', -43200),
    ('UTC-11:00', -39600),
    ('UTC-10:00', -36000),
    ('UTC-09:00', -32400),
    ('UTC-08:00', -28800),
    ('UTC-07:00', -25200),
    ('UTC-06:00', -21600),
    ('UTC-05:00', -18000),
    ('UTC-04:30', -16200),
    ('UTC-04:00', -14400),
    ('UTC-03:30', -12600),
    ('UTC-03:00', -10800),
    ('UTC-02:00',  -7200),
    ('UTC-01:00',  -3600),
    ('UTC-00:00',      0),
    ('UTC+00:00',      0),
    ('UTC+01:00',   3600),
    ('UTC+02:00',   7200),
    ('UTC+03:00',  10800),
    ('UTC+03:30',  12600),
    ('UTC+04:00',  14400),
    ('UTC+04:30',  16200),
    ('UTC+05:00',  18000),
    ('UTC+05:30',  19800),
    ('UTC+05:45',  20700),
    ('UTC+06:00',  21600),
    ('UTC+06:30',  23400),
    ('UTC+07:00',  25200),
    ('UTC+08:00',  28800),
    ('UTC+08:30',  30600),
    ('UTC+09:00',  32400),
    ('UTC+09:30',  34200),
    ('UTC+10:00',  36000),
    ('UTC+11:00',  39600),
    ('UTC+12:00',  43200),
    ('UTC+13:00',  46800),
    ('UTC+14:00',  50400),
)

### End of data that may need updates in response to CLDR ###

class ByteArrayData:
    def __init__(self):
        self.data = []
        self.hash = {}

    def append(self, s):
        s = s + '\0'
        if s in self.hash:
            return self.hash[s]

        lst = unicode2hex(s)
        index = len(self.data)
        if index > 0xffff:
            raise Error(f'Index ({index}) outside the uint16 range !')
        self.hash[s] = index
        self.data += lst
        return index

    def write(self, out, name):
        out(f'\nstatic const char {name}[] = {{\n')
        out(wrap_list(self.data))
        out('\n};\n')

class ZoneIdWriter (SourceFileEditor):
    def write(self, version, defaults, windowsIds):
        self.__writeWarning(version)
        windows, iana = self.__writeTables(self.writer.write, defaults, windowsIds)
        windows.write(self.writer.write, 'windowsIdData')
        iana.write(self.writer.write, 'ianaIdData')

    def __writeWarning(self, version):
        self.writer.write(f"""
/*
    This part of the file was generated on {datetime.date.today()} from the
    Common Locale Data Repository v{version} file supplemental/windowsZones.xml

    http://www.unicode.org/cldr/

    Do not edit this code: run cldr2qtimezone.py on updated (or
    edited) CLDR data; see qtbase/util/locale_database/.
*/

""")

    @staticmethod
    def __writeTables(out, defaults, windowsIds):
        windowsIdData, ianaIdData = ByteArrayData(), ByteArrayData()

        # Write Windows/IANA table
        out('// Windows ID Key, Territory Enum, IANA ID Index\n')
        out('static const QZoneData zoneDataTable[] = {\n')
        for index, data in sorted(windowsIds.items()):
            out('    {{ {:6d},{:6d},{:6d} }}, // {} / {}\n'.format(
                    data['windowsKey'], data['territoryId'],
                    ianaIdData.append(data['ianaList']),
                    data['windowsId'], data['territory']))
        out('    {      0,     0,     0 } // Trailing zeroes\n')
        out('};\n\n')

        # Write Windows ID key table
        out('// Windows ID Key, Windows ID Index, IANA ID Index, UTC Offset\n')
        out('static const QWindowsData windowsDataTable[] = {\n')
        for index, pair in enumerate(windowsIdList, 1):
            out('    {{ {:6d},{:6d},{:6d},{:6d} }}, // {}\n'.format(
                    index,
                    windowsIdData.append(pair[0]),
                    ianaIdData.append(defaults[index]),
                    pair[1], pair[0]))
        out('    {      0,     0,     0,     0 } // Trailing zeroes\n')
        out('};\n\n')

        # Write UTC ID key table
        out('// IANA ID Index, UTC Offset\n')
        out('static const QUtcData utcDataTable[] = {\n')
        for pair in utcIdList:
            out('    {{ {:6d},{:6d} }}, // {}\n'.format(
                    ianaIdData.append(pair[0]), pair[1], pair[0]))
        out('    {     0,      0 } // Trailing zeroes\n')
        out('};\n')

        return windowsIdData, ianaIdData

def usage(err, name, message=''):
    err.write(f"""Usage: {name} path/to/cldr/root path/to/qtbase
""") # TODO: more interesting message
    if message:
        err.write(f'\n{message}\n')

def main(args, out, err):
    """Parses CLDR's data and updates Qt's representation of it.

    Takes sys.argv, sys.stdout, sys.stderr (or equivalents) as
    arguments. Expects two command-line options: the root of the
    unpacked CLDR data-file tree and the root of the qtbase module's
    checkout. Updates QTimeZone's private data about Windows time-zone
    IDs."""
    name = args.pop(0)
    if len(args) != 2:
        usage(err, name, "Expected two arguments")
        return 1

    cldrPath = args.pop(0)
    qtPath = args.pop(0)

    if not os.path.isdir(qtPath):
        usage(err, name, f"No such Qt directory: {qtPath}")
        return 1
    if not os.path.isdir(cldrPath):
        usage(err, name, f"No such CLDR directory: {cldrPath}")
        return 1

    dataFilePath = os.path.join(qtPath, 'src', 'corelib', 'time', 'qtimezoneprivate_data_p.h')
    if not os.path.isfile(dataFilePath):
        usage(err, name, f'No such file: {dataFilePath}')
        return 1

    try:
        version, defaults, winIds = CldrAccess(cldrPath).readWindowsTimeZones(
            dict((name, ind) for ind, name in enumerate((x[0] for x in windowsIdList), 1)))
    except IOError as e:
        usage(err, name,
              f'Failed to open common/supplemental/windowsZones.xml: {e}')
        return 1
    except Error as e:
        err.write('\n'.join(textwrap.wrap(
                    f'Failed to read windowsZones.xml: {e}',
                    subsequent_indent=' ', width=80)) + '\n')
        return 1

    out.write('Input file parsed, now writing data\n')
    try:
        writer = ZoneIdWriter(dataFilePath, qtPath)
    except IOError as e:
        err.write(f'Failed to open files to transcribe: {e}')
        return 1

    try:
        writer.write(version, defaults, winIds)
    except Error as e:
        writer.cleanup()
        err.write(f'\nError in Windows ID data: {e}\n')
        return 1

    writer.close()
    out.write(f'Data generation completed, please check the new file at {dataFilePath}\n')
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv, sys.stdout, sys.stderr))
