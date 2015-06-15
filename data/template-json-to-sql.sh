#!/bin/bash

function print_sql {
	echo "UPDATE context_trigger_template SET j_template='{$2 }' WHERE name='$1';"
	echo "INSERT OR IGNORE INTO context_trigger_template (name, j_template) VALUES ('$1', '{$2 }');"
}

echo "CREATE TABLE IF NOT EXISTS context_trigger_template ("
echo "	name TEXT DEFAULT '' NOT NULL PRIMARY KEY,"
echo "	j_template TEXT DEFAULT '' NOT NULL);"

template_json_file="$1"

while read -r line
do
	keyword=`echo $line | awk '{print $1}' | tr -cd '[:alpha:]'`

	if [[ $keyword = "name" ]]; then

		if [ -n "$template" ]; then
			print_sql "$name" "$template"
			template=""
		fi

		name=`echo $line | tr -d '":,' | awk '{print $2}'`
	fi

	if [[ $keyword = "name" ]] || [[ $keyword = "attributes" ]] || [[ $keyword = "option" ]]; then
		template="$template $line"
	fi

done < "$template_json_file"

print_sql "$name" "$template"
