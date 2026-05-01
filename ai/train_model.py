"""
train_model.py — 训练异常检测模型并导出 C 代码
用法: python train_model.py
输入: labeled_data.csv（标注后的数据）
输出: ../src/ai_infer.h（C 语言推理头文件）
"""
import numpy as np
import pandas as pd
import os
import sys

from sklearn.ensemble import IsolationForest, RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.tree import export_text

CSV_PATH = os.path.join(os.path.dirname(__file__), "labeled_data.csv")
OUT_HEADER = os.path.join(os.path.dirname(__file__), "..", "src", "ai_infer.h")

def tree_to_c(tree, feature_names, indent=4):
    """将 sklearn 决策树翻译为 C if-else 代码"""
    tree_ = tree.tree_
    feature_name = [
        feature_names[i] if i != -2 else "undefined!"
        for i in tree_.feature
    ]

    lines = []
    def recurse(node, depth):
        indent_str = " " * (indent + depth * 4)
        if tree_.feature[node] != -2:
            name = feature_name[node]
            threshold = tree_.threshold[node]
            left = tree_.children_left[node]
            right = tree_.children_right[node]

            lines.append(f"{indent_str}if ({name} <= {threshold:.2f}) {{")
            recurse(left, depth + 1)
            lines.append(f"{indent_str}}} else {{")
            recurse(right, depth + 1)
            lines.append(f"{indent_str}}}")
        else:
            values = tree_.value[node][0]
            prediction = 0 if values[0] > values[1] else 1
            confidence = max(values) / sum(values)
            lines.append(f"{indent_str}return {prediction};  // {confidence:.1%}")

    recurse(0, 0)
    return "\n".join(lines)


def main():
    if not os.path.exists(CSV_PATH):
        print(f"错误: 找不到 {CSV_PATH}")
        print("请先运行 label_data.py 生成标注数据")
        sys.exit(1)

    df = pd.read_csv(CSV_PATH)
    print(f"数据量: {len(df)} 条")

    # 特征列（匹配你的 5 个传感器数据）
    feature_cols = ["temp", "humi", "vib_x", "vib_y", "vib_z"]
    X = df[feature_cols].values
    y = df["label"].values

    # ========== 方案 A: 无监督（只需正常数据）==========
    print("\n=== 方案 A: Isolation Forest (无监督) ===")
    normal_data = X[y == 0]
    iso_model = IsolationForest(contamination=0.05, random_state=42, n_estimators=100)
    iso_model.fit(normal_data)
    predictions = iso_model.predict(X)
    predictions = [1 if p == -1 else 0 for p in predictions]
    print(classification_report(y, predictions, target_names=["正常", "异常"], zero_division=0))

    # ========== 方案 B: 有监督（需要标注数据，更准确）==========
    print("\n=== 方案 B: Random Forest (有监督) ===")
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    clf = RandomForestClassifier(
        n_estimators=50,
        max_depth=6,
        random_state=42
    )
    clf.fit(X_train, y_train)

    y_pred = clf.predict(X_test)
    print(classification_report(y_test, y_pred, target_names=["正常", "异常"], zero_division=0))
    print("混淆矩阵:")
    print(confusion_matrix(y_test, y_pred))

    print("\n特征重要性:")
    for name, imp in zip(feature_cols, clf.feature_importances_):
        print(f"  {name}: {imp:.4f}")

    accuracy = clf.score(X_test, y_test)

    # ========== 导出 C 代码 ==========
    print("\n=== 导出 C 语言推理代码 ===")

    tree_rules = export_text(clf.estimators_[0], feature_names=feature_cols)
    print("决策树规则预览:")
    print(tree_rules[:500])

    c_code = f"""// ai_infer.h — AI 推理函数（由 Python 自动生成）
// 模型: Random Forest (简化为单棵决策树)
// 特征: {', '.join(feature_cols)}
// 训练数据量: {len(X)}
// 准确率: {accuracy:.1%}

#ifndef AI_INFER_H
#define AI_INFER_H

// 返回值: 0=正常, 1=异常
int ai_predict(float temp, float humi,
               int vib_x, int vib_y, int vib_z)
{{
{tree_to_c(clf.estimators_[0], feature_cols)}
}}

#endif
"""

    with open(OUT_HEADER, "w") as f:
        f.write(c_code)

    print(f"\n已生成 {OUT_HEADER}")
    print(f"准确率: {accuracy:.1%}")

    if accuracy < 0.85:
        print("\n⚠ 准确率低于 85%，建议:")
        print("  1. 增加训练数据量（尤其是异常样本）")
        print("  2. 在制造异常时让特征更明显")
        print("  3. 调整 label_data.py 中的标注阈值")

if __name__ == "__main__":
    main()
