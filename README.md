# flat JSON

flat JSON commands flat JSON file to flatten text file which treats by sed or awk easily.

## Commands

### flatj

flatj converts JSON file to flat text file.

```
$ cat e001.json
{ "key1": [ true, false, null ],
  "key2": { "key3": "string", "key4": "another" }}

$ flatj e001.json
key1:#0:true
key1:#1:false
key1:#2:null
key2:key3:string!
key2:key4:another!
```

### dflatj

dflatj converts flat text file to JSON file.  
dflatj does not print pretty. fmj command is available to print pretty.

```
# cat e001.flatj
key1:#0:true
key1:#1:false
key1:#2:null
key2:key3:string!
key2:key4:another!

$ dflatj e001.flatj
{"key1":[true,false,null],"key2":{"key3":"string","key4":"another"}}
```
### fmj

fmj prints JSON file pretty.

```
$ cat e001.json
{ "key1": [ true, false, null ],
  "key2": { "key3": "string", "key4": "another" }}

$ fmj e001.json
{
  "key1": [
    true,
    false,
    null
  ],
  "key2": {
    "key3": "string",
    "key4": "another"
  }
}
```

## Example

Print named character entity and its character from HTML Living Standard.

```
curl 'https://html.spec.whatwg.org/entities.json' |
flatj -E |
grep characters |
awk '{ print $1 "\t" $3 }'
```

Projection of JSON database.

```
flatj idols.json | egrep 'id|name|height' | dflatj | fmj
```

Result:
```json
[
  {
    "id": 1,
    "name": "KUDO Shinobu",
    "height": 154
  },
  {
    "id": 2,
    "name": "MOMOI Azuki",
    "height": 145
  },
  {
    "id": 3,
    "name": "AYASE Honoka",
    "height": 161
  },
  {
    "id": 4,
    "name": "KITAMI Yuzu",
    "height": 156
  }
]
```

where idols.json is shown as follows.

```json
[
  {
    "id": 1,
    "name": "KUDO Shinobu",
    "age": 16,
    "height": 154,
    "place": "Aomori"
  },
  {
    "id": 2,
    "name": "MOMOI Azuki",
    "age": 15,
    "height": 145,
    "place": "Nagano"
  },
  {
    "id": 3,
    "name": "AYASE Honoka",
    "age": 17,
    "height": 161,
    "place": "Miyagi"
  },
  {
    "id": 4,
    "name": "KITAMI Yuzu",
    "age": 15,
    "height": 156,
    "place": "Saitama"
  }
]
```

Compute average of a field.

```
flatj idols.json |
grep age |
awk '{ val += $NF + 0; line++ } END { print val / line }'
```

Result:

```
15.75
```

Convert JSON to CSV.

```
flatj idols.json | awk -F'\t' '{ print $NF }' | paste -d ',' - - - - -
```

Result:

```
1,KUDO Shinobu,16,154,Aomori
2,MOMOI Azuki,15,145,Nagano
3,AYASE Honoka,17,161,Miyagi
4,KITAMI Yuzu,15,156,Saitama
```

Select first record of JSON database.

```
flatj idols.json | sed '5q' | dflatj | fmj
```

Result:

```
[
  {
    "id": 1,
    "name": "KUDO Shinobu",
    "age": 16,
    "height": 154,
    "place": "Aomori"
  }
]
```

