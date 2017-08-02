#!/bin/bash
die(){
    echo >&2 "$@"
    exit 1
}
[ "$#" -eq 2 ] || die "usage: backup_db.sh [mysql_username] [output_file]"
mysqldump -u $1 -p --verbose --ignore-table=image_server.processing_jobs --ignore-table=image_server.processing_job_queue --ignore-table=image_server.alerts --ignore-table=image_server.host_event_log image_server | gzip -9 > "${2}"