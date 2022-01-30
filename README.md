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
curl 'https://html.spec.whatwg.org/entities.json' | flatj -E -s '' | grep characters | awk -F':' '{ print $1 "\t" $3 }'
```

