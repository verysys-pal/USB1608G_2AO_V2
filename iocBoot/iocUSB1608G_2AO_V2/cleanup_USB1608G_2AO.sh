#!/bin/bash

# EPICS 애플리케이션의 사용하지 않는 파일 및 빌드 아티팩트를 정리하는 스크립트

# 함수: 지정된 패턴의 파일/디렉토리를 찾아 삭제를 확인하고 실행
# 사용법: cleanup_items "설명" <find 명령어 인자들>
cleanup_items() {
    local description="$1"
    shift
    local find_args=("$@")

    echo "--- $description 검색 중..."
    # find 명령을 실행하고 결과를 배열에 저장
    # -print0과 read -d ''를 사용하여 공백이 포함된 파일 이름 처리
    local items_to_delete=()
    while IFS= read -r -d $'\0'; do
        items_to_delete+=("$REPLY")
    done < <(find . -depth "${find_args[@]}" -print0 2>/dev/null)


    if [ ${#items_to_delete[@]} -eq 0 ]; then
        echo " -> 삭제할 항목 없음."
        echo ""
        return
    fi

    echo " -> 다음 항목들을 삭제할까요?"
    printf '    - %s\n' "${items_to_delete[@]}"

    read -p "   삭제하시겠습니까? (y/n): " confirm
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        # 배열의 각 항목에 대해 rm -rf 실행
        for item in "${items_to_delete[@]}"; do
            if [ -e "$item" ]; then # 항목이 존재하는지 확인
                echo "    -> 삭제 중: $item"
                rm -rf "$item"
            fi
        done
        echo " -> 완료."
    else
        echo " -> 삭제를 건너뜁니다."
    fi
    echo ""
}

# 메인 정리 함수
main_cleanup() {
    echo "================================================="
    echo "EPICS 애플리케이션 정리 스크립트"
    echo "================================================="
    echo "현재 디렉토리: $(pwd)"
    echo ""

    # 1. 빌드 출력 디렉토리 (O.*)
    cleanup_items "빌드 출력 디렉토리 (O.*)" -type d -name "O.*"

    # 2. 백업 파일 (*.bak, *_BAK.adl)
    cleanup_items "백업 파일 (*.bak, *_BAK.adl)" -type f \( -name "*.bak" -o -name "*_BAK.adl" \)

    # 3. EPICS autosave 백업 파일 (*.sav[0-9], *.savB)
    cleanup_items "EPICS autosave 백업 파일" -type f \( -name "*.sav[0-9]" -o -name "*.savB" \)

    # 4. 컴파일된 실행 파일 (bin/*)
    cleanup_items "컴파일된 실행 파일" -path "./bin/*" -type f

    # 5. 이 IOC에서 사용하지 않는 다른 모델용 설정 파일 (*_settings.req)
    cleanup_items "다른 모델용 설정 파일" -type f -name "*_settings.req" \
        -not -name "measComp*" \
        -not -name "USB1608G_2AO_settings.req"

    echo "================================================="
    echo "정리 완료."
    echo "================================================="
}

# 스크립트 실행
main_cleanup
